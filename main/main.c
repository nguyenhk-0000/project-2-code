#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define drive_seat GPIO_NUM_11
#define pass_seat GPIO_NUM_12
#define drive_belt GPIO_NUM_13
#define pass_belt GPIO_NUM_17
#define ignite GPIO_NUM_18
#define buzzer GPIO_NUM_4
#define LED_RED GPIO_NUM_2 // Blue substitute
#define LED_GREEN GPIO_NUM_21

void app_main(void)
{
    /* ---------- GPIO CONFIGURATION ---------- */
    // Reset pins
    gpio_reset_pin(drive_seat);
    gpio_reset_pin(pass_seat);
    gpio_reset_pin(drive_belt);
    gpio_reset_pin(pass_belt);
    gpio_reset_pin(ignite);
    gpio_reset_pin(buzzer);
    gpio_reset_pin(LED_RED);
    gpio_reset_pin(LED_GREEN);

    // Inputs
    gpio_set_direction(drive_seat, GPIO_MODE_INPUT);
    gpio_set_direction(pass_seat, GPIO_MODE_INPUT);
    gpio_set_direction(drive_belt, GPIO_MODE_INPUT);
    gpio_set_direction(pass_belt, GPIO_MODE_INPUT);
    gpio_set_direction(ignite, GPIO_MODE_INPUT);

    // Internal pull-downs
    gpio_pulldown_en(drive_seat);
    gpio_pulldown_en(pass_seat);
    gpio_pulldown_en(drive_belt);
    gpio_pulldown_en(pass_belt);
    gpio_pulldown_en(ignite);

    gpio_pullup_dis(drive_seat);
    gpio_pullup_dis(pass_seat);
    gpio_pullup_dis(drive_belt);
    gpio_pullup_dis(pass_belt);
    gpio_pullup_dis(ignite);

    // Outputs
    gpio_set_direction(buzzer, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);    
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);

    gpio_set_level(buzzer, 0);
    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);

    /* ---------- STATE VARIABLES ---------- */
    bool welcome_shown = false;
    bool lastDriveSeat = false;
    
    // New state variables for Requirement 4 & 5
    bool engine_running = false; 
    bool last_ignition = false; 

    printf("System ready.\n");

    /* ---------- MAIN LOOP ---------- */
    while (1) {

        // Read Inputs
        bool driveSeat = gpio_get_level(drive_seat);
        bool passSeat  = gpio_get_level(pass_seat);
        bool driveBelt = gpio_get_level(drive_belt);
        bool passBelt  = gpio_get_level(pass_belt);
        bool ignition  = gpio_get_level(ignite);

        // Check if conditions are safe for ignition
        bool is_safe_to_start = (driveSeat && passSeat && driveBelt && passBelt);

        /* ---- 1. Welcome Message (once, on sit-down) ---- */
        // Req: Display "Welcome to enhanced alarm system model 218-W26" 
        if (driveSeat && !lastDriveSeat && !welcome_shown) {
            printf("Welcome to enhanced alarm system model 218-W26.\n");
            welcome_shown = true;
        }
        lastDriveSeat = driveSeat;

        /* ---- 2. Green LED: ignition enabled ---- */
        // Req: Indicate enabled only when safe 
        // logic: Turn ON Green ONLY if engine is OFF AND it is safe to start.
        // If engine is running, Green should be OFF[cite: 22].
        if (!engine_running && is_safe_to_start) {
            gpio_set_level(LED_GREEN, 1);
        } else {
            gpio_set_level(LED_GREEN, 0);
        }

        /* ---- 3, 4, 5. Ignition Button Logic ---- */
        // Detect button press (Rising Edge) to handle toggling
        if (ignition && !last_ignition) {
            
            if (engine_running) {
                /* ---- Requirement 5: Stop Engine ---- */
                // Req: When engine is running, stop engine when button pushed 
                engine_running = false;
                gpio_set_level(LED_RED, 0); // Turn off Engine LED
                printf("Engine stopped.\n");

            } else {
                /* ---- Requirement 3: Start Engine ---- */
                if (is_safe_to_start) {
                    // Req: If enabled (Green was lit), normal ignition [cite: 21]
                    engine_running = true;
                    gpio_set_level(LED_GREEN, 0); // Extinguish Green
                    gpio_set_level(LED_RED, 1);   // Light Engine LED
                    printf("Engine started.\n");  // [cite: 22]
                } else {
                    // Req: If not enabled, inhibit ignition [cite: 23]
                    // Req: Sound buzzer and display errors [cite: 23, 24]
                    gpio_set_level(buzzer, 1);
                    printf("Ignition inhibited\n"); // [cite: 24]

                    if (!driveSeat)  printf("Driver seat not occupied\n");
                    if (!passSeat)   printf("Passenger seat not occupied\n");
                    if (!driveBelt)  printf("Driver seatbelt not fastened\n");
                    if (!passBelt)   printf("Passenger seatbelt not fastened\n");
                    
                    // Small delay for buzzer so it beeps audibly then turns off
                    vTaskDelay(200 / portTICK_PERIOD_MS); 
                    gpio_set_level(buzzer, 0);
                    
                    // Req: Allow additional start attempts (Loop continues naturally) 
                }
            }
        }
        
        last_ignition = ignition; // Update last state for edge detection

        /* ---- Requirement 4: Engine Latch ---- */
        // Req: Keep engine running even if belts removed 
        // This is handled because `engine_running` stays true until the button is pressed again.
        // We ensure the RED LED reflects this state every loop.
        gpio_set_level(LED_RED, engine_running ? 1 : 0);

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}