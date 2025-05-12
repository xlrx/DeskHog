#include "flappy_bird.h"
#include "Arduino.h" // For random, etc.
#include "Style.h"   // For Style::loudNoisesFont()

FlappyBirdGame::FlappyBirdGame()
    : main_container(nullptr), bird_obj(nullptr), 
      start_message_label(nullptr), game_over_message_label(nullptr), score_label(nullptr),
      bird_y(FB_SCREEN_HEIGHT / 2), bird_velocity(0.0f), 
      current_game_state(GameState::PRE_GAME), score(0) {
    Serial.println("[FlappyBird] Constructor called"); // DEBUG
    for (int i = 0; i < PIPE_COUNT; ++i) {
        pipes[i].top_asterisk_obj = nullptr;     // Initialize new member
        pipes[i].bottom_asterisk_obj = nullptr;  // Initialize new member
        pipes[i].scored = false;
    }
}

FlappyBirdGame::~FlappyBirdGame() {
    Serial.println("[FlappyBird] Destructor called"); // DEBUG
    cleanup();
}

void FlappyBirdGame::setup(lv_obj_t* parent_screen) {
    Serial.println("[FlappyBird] setup() called"); // DEBUG
    current_game_state = GameState::PRE_GAME;
    score = 0;
    Serial.printf("[FlappyBird] Game state set to PRE_GAME, score reset to %d\n", score); // DEBUG

    if (!main_container) { 
        Serial.println("[FlappyBird] Creating main_container"); // DEBUG
        main_container = lv_obj_create(parent_screen);
        lv_obj_remove_style_all(main_container);
        lv_obj_set_size(main_container, FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(main_container, lv_color_hex(0x87CEEB), LV_PART_MAIN); 
        lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, LV_PART_MAIN); // Ensure opaque background
        lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (!bird_obj) { 
        Serial.println("[FlappyBird] Creating bird_obj"); // DEBUG
        bird_obj = lv_obj_create(main_container);
        lv_obj_set_size(bird_obj, BIRD_SIZE, BIRD_SIZE);
        lv_obj_set_style_bg_color(bird_obj, lv_color_hex(0xFFD700), LV_PART_MAIN);
        lv_obj_set_style_radius(bird_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_align(bird_obj, LV_ALIGN_CENTER, BIRD_X_POSITION - FB_SCREEN_WIDTH/2 , 0); 
    }
    bird_y = FB_SCREEN_HEIGHT / 2;
    bird_velocity = 0.0f;
    lv_obj_set_y(bird_obj, bird_y - BIRD_SIZE / 2);
    Serial.printf("[FlappyBird] Bird initialized: y=%d, velocity=%.2f\n", bird_y, bird_velocity); // DEBUG

    reset_and_initialize_pipes();

    if (!start_message_label) {
        Serial.println("[FlappyBird] Creating start_message_label"); // DEBUG
        start_message_label = lv_label_create(main_container);
        lv_obj_set_style_text_font(start_message_label, Style::loudNoisesFont(), 0);
        lv_obj_set_style_text_color(start_message_label, lv_color_white(), 0); // Set text color to white
        lv_label_set_text(start_message_label, "Press Center!");
        lv_obj_align(start_message_label, LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_clear_flag(start_message_label, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[FlappyBird] Start message visible"); // DEBUG

    if (game_over_message_label) {
        lv_obj_add_flag(game_over_message_label, LV_OBJ_FLAG_HIDDEN);
        Serial.println("[FlappyBird] Game Over message hidden"); // DEBUG
    }

    if (!score_label) {
        Serial.println("[FlappyBird] Creating score_label"); // DEBUG
        score_label = lv_label_create(main_container);
        lv_obj_set_style_text_font(score_label, Style::loudNoisesFont(), 0);
        lv_obj_set_style_text_color(score_label, lv_color_white(), 0); // Also set score color to white for consistency
        lv_obj_align(score_label, LV_ALIGN_TOP_LEFT, 5, 5);
    }
    lv_label_set_text_fmt(score_label, "Score: %d", score);
    lv_obj_clear_flag(score_label, LV_OBJ_FLAG_HIDDEN); 
}

void FlappyBirdGame::reset_and_initialize_pipes() {
    Serial.println("[FlappyBird] reset_and_initialize_pipes() called"); // DEBUG
    srand(time(NULL)); 
    for (int i = 0; i < PIPE_COUNT; ++i) {
        pipes[i].x_position = FB_SCREEN_WIDTH + (i * HORIZONTAL_SPACING_BETWEEN_PIPES);
        int available_height_for_gap = FB_SCREEN_HEIGHT - (2 * MIN_PIPE_HEIGHT) - PIPE_GAP_HEIGHT;
        if(available_height_for_gap < 0) available_height_for_gap = 0;
        pipes[i].gap_y_top = MIN_PIPE_HEIGHT + (rand() % (available_height_for_gap + 1));
        pipes[i].scored = false;

        // Top asterisk
        if (!pipes[i].top_asterisk_obj) {
            pipes[i].top_asterisk_obj = lv_label_create(main_container);
            lv_label_set_text(pipes[i].top_asterisk_obj, "*");
            lv_obj_set_style_text_font(pipes[i].top_asterisk_obj, Style::loudNoisesFont(), 0); // Or a smaller font
            lv_obj_set_style_text_color(pipes[i].top_asterisk_obj, lv_color_hex(0xFF0000), 0); // Red
        }
        lv_obj_set_pos(pipes[i].top_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top);
        // Serial.printf("[FlappyBird] Pipe %d Top Asterisk: x=%d, y=%d\n", i, (int)pipes[i].x_position, pipes[i].gap_y_top);

        // Bottom asterisk
        if (!pipes[i].bottom_asterisk_obj) {
            pipes[i].bottom_asterisk_obj = lv_label_create(main_container);
            lv_label_set_text(pipes[i].bottom_asterisk_obj, "*");
            lv_obj_set_style_text_font(pipes[i].bottom_asterisk_obj, Style::loudNoisesFont(), 0); // Or a smaller font
            lv_obj_set_style_text_color(pipes[i].bottom_asterisk_obj, lv_color_hex(0xFF0000), 0); // Red
        }
        lv_obj_set_pos(pipes[i].bottom_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top + PIPE_GAP_HEIGHT);
        // Serial.printf("[FlappyBird] Pipe %d Bottom Asterisk: x=%d, y=%d\n", i, (int)pipes[i].x_position, pipes[i].gap_y_top + PIPE_GAP_HEIGHT);
    }
}

void FlappyBirdGame::loop() {
    // Serial.printf("[FlappyBird] loop(), state: %d\n", (int)current_game_state); // DEBUG - Very noisy
    switch (current_game_state) {
        case GameState::PRE_GAME:
            if (Input::isCenterPressed()) {
                Serial.println("[FlappyBird] Center pressed in PRE_GAME - starting game"); // DEBUG
                current_game_state = GameState::ACTIVE;
                if (start_message_label) {
                    lv_obj_add_flag(start_message_label, LV_OBJ_FLAG_HIDDEN);
                }
                bird_y = FB_SCREEN_HEIGHT / 2;
                bird_velocity = 0.0f;
            }
            break;

        case GameState::ACTIVE:
            handle_input();      
            update_game_state(); 
            render();            
            break;

        case GameState::GAME_OVER:
            if (Input::isCenterPressed()) {
                Serial.println("[FlappyBird] Center pressed in GAME_OVER - restarting to PRE_GAME"); // DEBUG
                setup(lv_obj_get_parent(main_container)); 
            }
            break;
    }
}

void FlappyBirdGame::handle_input() {
    if (current_game_state != GameState::ACTIVE) return;
    if (Input::isCenterPressed()) {
        Serial.println("[FlappyBird] Flap! (Center pressed in ACTIVE state)"); // DEBUG
        bird_velocity = -1.25f;
    }
}

void FlappyBirdGame::update_game_state() {
    if (current_game_state != GameState::ACTIVE) return;

    bird_velocity += 0.05f;
    bird_y += (int)bird_velocity;

    // Clamp bird's position to prevent going off the top of the screen
    if (bird_y - BIRD_SIZE / 2 < 0) {
        bird_y = BIRD_SIZE / 2; // Position bird at the very top
        bird_velocity = 0;      // Stop upward movement if it hit the top
        Serial.println("[FlappyBird] Bird hit top, clamped position."); // DEBUG
    }
    // Serial.printf("[FlappyBird] Bird update: y=%d, vel=%.2f\n", bird_y, bird_velocity); // DEBUG - Noisy

    // Bird-Screen TOP or BOTTOM boundary collision
    if (bird_y - BIRD_SIZE / 2 <= 0 || bird_y + BIRD_SIZE / 2 >= FB_SCREEN_HEIGHT) { // Re-added top boundary check and used >= / <= for robustness
        Serial.println("[FlappyBird] Top/Bottom boundary collision! Game Over."); // DEBUG: Updated message
        current_game_state = GameState::GAME_OVER;
        if (!game_over_message_label) {
            game_over_message_label = lv_label_create(main_container);
            lv_obj_set_style_text_font(game_over_message_label, Style::loudNoisesFont(), 0);
            lv_obj_set_style_text_color(game_over_message_label, lv_color_white(), 0); // Set text color to white
            lv_label_set_text(game_over_message_label, "Game Over!");
            lv_obj_align(game_over_message_label, LV_ALIGN_CENTER, 0, 0); 
        }
        lv_obj_clear_flag(game_over_message_label, LV_OBJ_FLAG_HIDDEN);
        if (start_message_label) lv_obj_add_flag(start_message_label, LV_OBJ_FLAG_HIDDEN);
        return; 
    }

    for (int i = 0; i < PIPE_COUNT; ++i) {
        pipes[i].x_position -= PIPE_MOVE_SPEED;
        // Serial.printf("[FlappyBird] Pipe %d moved to x=%.2f\n", i, pipes[i].x_position); // DEBUG - Noisy

        int bird_left = BIRD_X_POSITION - BIRD_SIZE / 2;
        int bird_right = BIRD_X_POSITION + BIRD_SIZE / 2;
        int bird_top = bird_y - BIRD_SIZE / 2;
        int bird_bottom = bird_y + BIRD_SIZE / 2;
        int pipe_left = (int)pipes[i].x_position;
        int pipe_right = (int)pipes[i].x_position + PIPE_WIDTH;
        int top_pipe_bottom_edge = pipes[i].gap_y_top;
        int bottom_pipe_top_edge = pipes[i].gap_y_top + PIPE_GAP_HEIGHT;

        if (bird_right > pipe_left && bird_left < pipe_right) {
            // Serial.printf("[FlappyBird] X-Overlap with Pipe %d: Bird (T:%d, B:%d), PipeGap (T:%d, B:%d)\n", 
            //               i, bird_top, bird_bottom, top_pipe_bottom_edge, bottom_pipe_top_edge); // DEBUG - Can be noisy
            
            if (bird_top < top_pipe_bottom_edge || bird_bottom > bottom_pipe_top_edge) {
                Serial.printf("[FlappyBird] Pipe %d Collision! Bird(T:%d, B:%d) vs PipeGap(T:%d, B:%d). Game Over.\n", 
                              i, bird_top, bird_bottom, top_pipe_bottom_edge, bottom_pipe_top_edge); // DEBUG
                current_game_state = GameState::GAME_OVER;
            }
        }

        if (current_game_state == GameState::GAME_OVER) {
             if (!game_over_message_label) {
                game_over_message_label = lv_label_create(main_container);
                lv_obj_set_style_text_font(game_over_message_label, Style::loudNoisesFont(), 0);
                lv_obj_set_style_text_color(game_over_message_label, lv_color_white(), 0); // Set text color to white
                lv_label_set_text(game_over_message_label, "Game Over!");
                lv_obj_align(game_over_message_label, LV_ALIGN_CENTER, 0, 0); 
            }
            lv_obj_clear_flag(game_over_message_label, LV_OBJ_FLAG_HIDDEN);
            if (start_message_label) lv_obj_add_flag(start_message_label, LV_OBJ_FLAG_HIDDEN);
            return; 
        }

        if (!pipes[i].scored && bird_left > pipe_right) {
            score++;
            pipes[i].scored = true;
            Serial.printf("[FlappyBird] Pipe %d scored! Score: %d\n", i, score); // DEBUG
        }

        if (pipes[i].x_position + PIPE_WIDTH < 0) {
            // Serial.printf("[FlappyBird] Recycling pipe %d\n", i); // DEBUG - Can be noisy
            float max_x = 0;
            for(int j=0; j < PIPE_COUNT; ++j) {
                if (j == i) continue;
                if (pipes[j].x_position > max_x) max_x = pipes[j].x_position;
            }
            pipes[i].x_position = max_x + HORIZONTAL_SPACING_BETWEEN_PIPES;

            int available_height_for_gap = FB_SCREEN_HEIGHT - (2 * MIN_PIPE_HEIGHT) - PIPE_GAP_HEIGHT;
            if(available_height_for_gap < 0) available_height_for_gap = 0;
            pipes[i].gap_y_top = MIN_PIPE_HEIGHT + (rand() % (available_height_for_gap + 1));
            pipes[i].scored = false;
            // Serial.printf("[FlappyBird] Pipe %d recycled: x=%.2f, gap_top=%d\n", i, pipes[i].x_position, pipes[i].gap_y_top); // DEBUG

            lv_obj_set_pos(pipes[i].top_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top);
            lv_obj_set_pos(pipes[i].bottom_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top + PIPE_GAP_HEIGHT);
        }
    }
}

void FlappyBirdGame::render() {
    // Serial.println("[FlappyBird] render() called"); // DEBUG - Very noisy
    if (bird_obj) {
        lv_obj_set_y(bird_obj, bird_y - BIRD_SIZE / 2);
    }

    for (int i = 0; i < PIPE_COUNT; ++i) {
        if (pipes[i].top_asterisk_obj) {
            lv_obj_set_pos(pipes[i].top_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top);
        }
        if (pipes[i].bottom_asterisk_obj) {
            lv_obj_set_pos(pipes[i].bottom_asterisk_obj, (int)pipes[i].x_position, pipes[i].gap_y_top + PIPE_GAP_HEIGHT);
        }
    }

    if (score_label) {
        lv_label_set_text_fmt(score_label, "Score: %d", score);
    }
}

void FlappyBirdGame::cleanup() {
    Serial.println("[FlappyBird] cleanup() called"); // DEBUG
    if (main_container) {
        lv_obj_del(main_container); 
        main_container = nullptr;
        bird_obj = nullptr; 
        start_message_label = nullptr;
        game_over_message_label = nullptr;
        score_label = nullptr;
        for (int i = 0; i < PIPE_COUNT; ++i) {
            pipes[i].top_asterisk_obj = nullptr;     // Null out new members
            pipes[i].bottom_asterisk_obj = nullptr;  // Null out new members
        }
    }
}

lv_obj_t* FlappyBirdGame::get_main_container() {
    return main_container;
} 