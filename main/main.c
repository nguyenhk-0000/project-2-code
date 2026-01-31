#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* ---------- GPIO DEFINITIONS ---------- */
#define drive_seat GPIO_NUM_15
#define pass_seat GPIO_NUM_16
#define drive_belt GPIO_NUM_17
#define pass_belt GPIO_NUM_18
#define ignite GPIO_NUM_10
#define buzzer GPIO_NUM_40
#define LED_RED GPIO_NUM_41 // Blue substitute
#define LED_GREEN GPIO_NUM_42

#define LED_LEFT_LOW_BEAM GPIO_NUM_39
#define LED_RIGHT_LOW_BEAM GPIO_NUM_38

/* ---------- ADC DEFINITIONS ---------- */
#define HEADLIGHT_MODE_DETECTOR ADC_CHANNEL_3   //potentiometer
#define LIGHT_SENSOR ADC_CHANNEL_4

#define ADC_ATTEN       ADC_ATTEN_DB_12
#define BITWIDTH        ADC_BITWIDTH_12
#define BUF_SIZE        16                  // Number of points to average the ADC input voltage

/* ---------- THRESHOLDS ---------- */
// Threshold values for light sensor
int daylight = 1722;
int dusk = 880;

// Threshold values for potentiometer modes
int mode_off_max_mv = 1365;
int mode_on_min_mv = 2730;

//maybe helper function but optional

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

    /* ---------- ADC INITIALIZATION ---------- */
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };                                                  // Unit configuration
    adc_oneshot_unit_handle_t adc1_handle;              // Unit handle
    adc_oneshot_new_unit(&init_config1, &adc1_handle);  // Populate unit handle

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = BITWIDTH
    };                                                  // Channel config
    adc_oneshot_config_channel(adc1_handle, HEADLIGHT_MODE_DETECTOR, &chan_config);
    adc_oneshot_config_channel(adc1_handle, LIGHT_SENSOR, &chan_config);

    adc_cali_handle_t pot_cali_handle;                  // Calibration handle for potentiometer
    adc_cali_handle_t light_cali_handle;                // Calibration handle for light sensor

    adc_cali_curve_fitting_config_t pot_cali = {        // Calibration config for potentiometer
        .unit_id = ADC_UNIT_1,
        .chan = HEADLIGHT_MODE_DETECTOR,
        .atten = ADC_ATTEN,
        .bitwidth = BITWIDTH
    };

    adc_cali_curve_fitting_config_t light_cali = {      // Calibration config for light sensor
        .unit_id = ADC_UNIT_1,
        .chan = LIGHT_SENSOR,
        .atten = ADC_ATTEN,
        .bitwidth = BITWIDTH
    };

    adc_cali_create_scheme_curve_fitting(&pot_cali, &pot_cali_handle);      // Populate cal handle for pot.
    adc_cali_create_scheme_curve_fitting(&light_cali, &light_cali_handle);  //Populate cal handle for light sen.

    /* ---------- FILTER BUFFERS ---------- */
    //store values that will be smoothed the ADC output for both sensors
    int pot_buffer[BUF_SIZE] = {0};
    int light_buffer[BUF_SIZE] = {0};
    int pot_index = 0;
    int light_index = 0;

    /* ---------- STATE VARIABLES ---------- */
    bool welcome_shown = false;
    bool lastDriveSeat = false;
    
    // New state variables for Requirement 4 & 5
    bool engine_running = false; 
    bool last_ignition = false; 

    // New state variables for Requirement 6-8
    bool headlights_on = true;
    int dusk_counter = 0;      // AUTO mode counter toward turning ON
    int dayLi_counter = 0;       // AUTO mode counter toward turning OFF


    printf("System ready.\n");

    /* ---------- MAIN LOOP ---------- */
    while (1) {

        // Read Inputs
        bool driveSeat = gpio_get_level(drive_seat);
        bool passSeat  = gpio_get_level(pass_seat);
        bool driveBelt = gpio_get_level(drive_belt);
        bool passBelt  = gpio_get_level(pass_belt);
        bool ignition  = gpio_get_level(ignite);

        /* ---------- POTENTIOMETER READ + FILTER ---------- */
        int pot_bits, pot_mv;               // ADC reading (bits and mV)
        adc_oneshot_read(adc1_handle, HEADLIGHT_MODE_DETECTOR, &pot_bits);    // Read ADC bits
        adc_cali_raw_to_voltage(pot_cali_handle, pot_bits, &pot_mv);          // Convert to mV

        // Store the new voltage reading into the circular buffer.
        pot_buffer[pot_index] = pot_mv;
        pot_index = (pot_index + 1) % BUF_SIZE;

        // Compute the average filtered value.
        int pot_filt = 0;
        for (int i = 0; i < BUF_SIZE; i++) pot_filt += pot_buffer[i];
        pot_filt /= BUF_SIZE;

        /* ---------- LIGHT SENSOR READ + FILTER ---------- */
        int light_bits, light_mv;           // ADC reading (bits and mV)
        adc_oneshot_read(adc1_handle, LIGHT_SENSOR, &light_bits);           // Read ADC bits
        adc_cali_raw_to_voltage(light_cali_handle, light_bits, &light_mv);  // Convert to mV

        // Store the new voltage reading into the circular buffer.
        light_buffer[light_index] = light_mv;
        light_index = (light_index + 1) % BUF_SIZE;

        // Compute the average filtered value.
        int light_filt = 0;
        for (int i = 0; i < BUF_SIZE; i++) light_filt += light_buffer[i];
        light_filt /= BUF_SIZE;

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
        
        last_ignition = ignition; // Update last state for edge detection

        /* Requirement 4: Engine Latch */
       
        // This ensures the hardware LED always reflects the engine's software state.
        if (engine_running == true) {
            gpio_set_level(LED_RED, 1); // Keep the engine LED on [cite: 22, 26]
        } else {
            gpio_set_level(LED_RED, 0); // Keep the engine LED off [cite: 27, 36]
        }

        /* ---- 6, 7, 8. Headlight subsystem logic ---- */
        // Only implement this subsytem if the engine is activelly running.
        // Otherwise, turn headlights off.
        if (engine_running) {

            if (pot_filt >=mode_on_min_mv) {               // If mode selected is ON.
                headlights_on = true;
                dusk_counter = 0;
                dayLi_counter = 0;
                //printf("Mode ON\n");
            } else if (pot_filt <= mode_off_max_mv) {        // If mode selected is OFF.
                headlights_on = false;
                dusk_counter = 0;
                dayLi_counter = 0;  
                //printf("Mode OFF\n");          
            } else {                                    // If mode selected is AUTO.
                printf("Mode AUTO\n");
                if (light_filt < dusk) {
                    dusk_counter++;
                    dayLi_counter = 0;

                    // A delay of 1 second calculated taking advantage of loop delay (50ms)
                    if (dusk_counter >= 20) {   // 20 iterations × 50 ms = 1000 ms = 1s
                        headlights_on = true;
                    }
                    //printf("Less than dusk --> Turn lights ON\n");
                } else if (light_filt > daylight) {
                    dayLi_counter++;
                    dusk_counter = 0;

                    // A delay of 2 seconds calculated taking advantage of loop delay (50ms)
                    if (dayLi_counter >= 40) { // 40 iterations × 50 ms = 2000 ms = 2s
                        headlights_on = false;
                    }
                    //printf("More than daylight --> Turn lights OFF\n");
                } else {   // if light filter value is somewhere between, maintain previous state
                    dusk_counter = 0;
                    dayLi_counter = 0;
                    //printf("Neither nor --> Maintain prev state\n");
                }
            }

            // Output headlight state
            gpio_set_level(LED_LEFT_LOW_BEAM, headlights_on);
            gpio_set_level(LED_RIGHT_LOW_BEAM, headlights_on);
        } else {
            headlights_on = false;
            dusk_counter = 0;
            dayLi_counter = 0;
            gpio_set_level(LED_LEFT_LOW_BEAM, 0);
            gpio_set_level(LED_RIGHT_LOW_BEAM, 0);
        }

        // loop delay
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}