#include <limits.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <cpu.h>
#include <io.h>

#define SCREEN_WIDTH 224
#define SCREEN_HEIGHT 256
SDL_Window* window;
SDL_Renderer* renderer;
static bool pending_interrupt = 0;
static enum OpCode next_interrupt = RST_1;

#define VRAM_START  0x2400U
#define VRAM_LENGTH 0x1C00U

size_t counter_clockwise_rotate(size_t vram_pixel_index)
{
    const int width = SCREEN_WIDTH;
    const int height = SCREEN_HEIGHT;
    return (height - 1 - (vram_pixel_index % height)) * width + (vram_pixel_index / height);
}

SDL_Texture *texture;
void render_screen()
{
    SDL_RenderClear(renderer);
    if (!texture) texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              SCREEN_WIDTH, SCREEN_HEIGHT);


    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
        fprintf(stderr, "Texture lock failed (%s)\n", SDL_GetError());
        exit(2);
    }

    uint32_t *pixelptr = (uint32_t*)pixels;
    size_t pixels_read = 0;
    // 0x1C00 //
    /* a conversion procedure
     * read in Space Invaders VRAM, output valid pixel data */
    for (size_t i = 0; i < VRAM_LENGTH; ++i) {
        uint8_t byte = memory[VRAM_START + i];
        for (size_t j = 0; j < CHAR_BIT; ++j) {
            bool bit = byte & 0x01;
            byte >>= 1;
            /* in Space Invaders: address 0 of VRAM represents the lower left of the screen
             * in SDL:            address 0 of pixel data represents the top left of the screen */
            pixelptr[counter_clockwise_rotate(pixels_read)] = 0xFF000000 + (0x00FFFFFF * bit);
            ++pixels_read;
        }
    }

    SDL_UnlockTexture(texture);

    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int main()
{

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Space Invaders",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, 0);

    renderer = SDL_CreateRenderer(window, 0, 0);
    if (!renderer) {
        printf("%s\n", SDL_GetError());
        return 1;
    }

    load_rom(memory, MEM_SIZE, "invaders");
    SDL_Event e;
    uint32_t curr_ticks, last_draw;
    curr_ticks = last_draw = SDL_GetTicks();
    while (1) {
        curr_ticks = SDL_GetTicks();
        /* Handle SDL events */
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                goto cleanup;
            }
        }
        /* Handle interrupts */
        if (interrupt_enabled && pending_interrupt) {
            instruction(next_interrupt);
            pending_interrupt = 0;
        }
        enum OpCode opcode = read_next_byte();
        if (instruction(opcode)) {
            goto cleanup;
        }
        if (curr_ticks - last_draw >= 10) {
            last_draw = SDL_GetTicks();
            /* generate interrupt */
            if (next_interrupt == RST_1) {
                next_interrupt = RST_2;
                render_screen();
            } else {
                next_interrupt = RST_1;
            }
            pending_interrupt = 1;
        }
    }
    cleanup:
    SDL_DestroyTexture(texture);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
