#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define drive_seat GPIO_NUM_15
#define pass_seat GPIO_NUM_16
#define drive_belt GPIO_NUM_17
#define pass_belt GPIO_NUM_18
#define ignite GPIO_NUM_10
#define buzzer GPIO_NUM_12
#define LED_RED GPIO_NUM_14 // Blue substitute
#define LED_GREEN GPIO_NUM_13
#define LED_LEFT_LOW_BEAM GPIO_NUM_38
#define LED_RIGHT_LOW_BEAM GPIO_NUM_39
//ADC channel 3/GPIO4 - potentiometer
//ADC channel 4/GPIO5 -light sensor
//#define LED_SENSOR GPIO_NUM_24
//#define LIGHTS_MODE_SELECTOR GPIO_NUM_25

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
    gpio_reset_pin(LED_LEFT_LOW_BEAM);
    gpio_reset_pin(LED_RIGHT_LOW_BEAM);

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
    gpio_set_direction(LED_LEFT_LOW_BEAM, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RIGHT_LOW_BEAM, GPIO_MODE_OUTPUT);

    gpio_set_level(buzzer, 0);
    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_LEFT_LOW_BEAM, 0);
    gpio_set_level(LED_RIGHT_LOW_BEAM, 0);

    /* ---------- STATE VARIABLES ---------- */
    bool welcome_shown = false;
    bool lastDriveSeat = false;
    
    // New state variables for Requirement 4 & 5
    bool engine_running = false; 
    bool last_ignition = false; 

    // New state variable for Requirements ???
    char headlightMode[] = ""; // ON, OFF or AUTO modes
        //will need a helper function to convert int values from potentiometer 
        //to strings (optional)
    // Threshhold values
    int daylight = 1365; //change to appropriate value
    int dusk = 2730;     //change to appropriate value

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

        /* 1. Welcome Message (once, on sit-down) */
        if (driveSeat && !lastDriveSeat && !welcome_shown) {
            printf("Welcome to enhanced alarm system model 218-W26.\n");
            welcome_shown = true;
        }
        lastDriveSeat = driveSeat;

        /* ---- 2. Green LED: ignition enabled ---- */
        if (!engine_running && is_safe_to_start) {
            gpio_set_level(LED_GREEN, 1);
        } else {
            gpio_set_level(LED_GREEN, 0);
        }

        /* ---- 3, 4, 5. Ignition Button Logic ---- */
        if (ignition && !last_ignition) {
            
            if (engine_running) {
                engine_running = false;
                gpio_set_level(LED_RED, 0); // Turn off Engine LED
                printf("Engine stopped.\n");

            } else {
                /*3: Start Engine */
                if (is_safe_to_start) {
                    // Req: If enabled (Green was lit), normal ignition 
                    engine_running = true;
                    gpio_set_level(LED_GREEN, 0); // Extinguish Green
                    gpio_set_level(LED_RED, 1);   // Light Engine LED
                    printf("Engine started.\n");  
                } else {
                    // Req: If not enabled, inhibit ignition 
                    // Req: Sound buzzer and display errors 
                    gpio_set_level(buzzer, 1);
                    printf("Ignition inhibited\n");

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

        /* ---- 6, 7, 8. Headlight subsystem logic ---- */
        /* To finish & test:
        if (engine_running) {
            if (headlightMode=="ON") {
                //turn on both low beam lamps
            } else if (headlightMode=="OFF") {
                //turn off both low beam lamps 
            } else if (headlightMode=="AUTO") {
                if (lightLevel >= daylight) {
                    vTaskDelay(2000 / portTICK_PERIOD_MS); //2 sec delay
                    //low beam lamps turn off
                }  else if (lightLevel <= dusk) {
                    vTaskDelay(1000 / portTICK_PERIOD_MS); //1 sec delay
                    //low beam lamps turn on
                } else {
                    //maintain prev state 
                }
            }
        } else {
            //turn off all low beam lamps 
        }
        
        */



        last_ignition = ignition; // Update last state for edge detection

        /* Requirement 4: Engine Latch */
       
        // This ensures the hardware LED always reflects the engine's software state.
        if (engine_running == true) {
            gpio_set_level(LED_RED, 1); // Keep the engine LED on [cite: 22, 26]
        } else {
            gpio_set_level(LED_RED, 0); // Keep the engine LED off [cite: 27, 36]
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
            
    }
}
