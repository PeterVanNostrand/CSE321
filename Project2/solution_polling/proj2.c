#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/iotn85.h>

// Location of the bit in DDRB, PORTB, and PINB associated 
// with the corresponding pin. Used for masking pin values
#define PIN2B (1 << PB3)
#define PIN3B (1 << PB4)
#define PIN5B (1 << PB0)
#define PIN6B (1<<PB1)
#define PIN7B (1<<PB2)

uint8_t state = 0;
uint8_t button_1_history = 0;
uint8_t button_2_history = 0;
uint8_t entered_combo = 0;
uint8_t stored_combo = 0;
uint8_t combo_presses = 0;

/*
 * Button 1 is connected to pin 2 and represents a 0 entry for the combination
 * Button 2 is connected to pin 3 and represents a 1 entry for the combination
*/

void button_pressed(){
    // save current output states
    uint8_t old_DDRB = DDRB;
    uint8_t old_PORTB = PORTB;
    // flash BLUE LED to indicate button press registered
    DDRB = PIN6B | PIN7B;
    PORTB = PIN6B;
    _delay_ms(300);
    // restore output states
    DDRB = old_DDRB;
    PORTB = old_PORTB;
}

void incorrect_combo(){
    // clear incorrect password
    combo_presses = 0;
    entered_combo = 0;

    // Flash the RED and YELLOW LEDs to indicate incorrect combo
    for(uint8_t i=0; i<10; i++){
        //Turn on just YELLOW LED
        DDRB = PIN5B | PIN6B;
        PORTB = PIN6B;
        _delay_ms(25);
        //Turn on just RED LED
        DDRB = PIN6B | PIN7B;
        PORTB = PIN7B;
        _delay_ms(25);
    }
}

void lock(){
    // save the entered combo
    stored_combo = entered_combo;
    // reset temporary fields to recieve first unlock attempt
    entered_combo = 0;
    combo_presses = 0;
    //set the state to locked and turn on the RED LED
    state = 1;
    DDRB = PIN6B | PIN7B;
    PORTB = PIN7B;
}

void unlock(){
    // clear the saved combination
    stored_combo = 0;
    // reset temporary fields to recieve a new combo
    entered_combo = 0;
    combo_presses = 0;
    // set the state to unlocked and turn on the GREEN LED
    state = 0;
    DDRB = PIN5B | PIN6B;
    PORTB = PIN5B;
}

/*
 * Polls the state of the buttons using debounce logic.  Only one button may be pressed in a given cycle.
 * If the debounce logic indicates that a button is actually pressed it stores the button's value in the 
 * "entered_combo" field and calls "button_pressed()" to indicate the button press was registered
*/
void check_buttons(){
    // take a new sample. assuming no button pressed the shift operator appends a 0 to the history
    button_1_history <<= 1;
    button_2_history <<= 1;
    // if a button is pressed replace that 0 with a 1
    if(!(PINB&PIN2B))
        button_1_history |= 1;
    else if(!(PINB&PIN3B))
        button_2_history |= 1;

    if(button_1_history==0xFF){ // button 1 has been pressed for 8 cycles
        entered_combo <<= 1; // shift a 0 into the combo
        combo_presses++;
        button_pressed();
    }
    else if(button_2_history==0xFF){ // button 2 pressed has been pressed for 8 cycles
        entered_combo <<= 1;
        entered_combo |= 1; // shift a 1 into the combo
        combo_presses++;
        button_pressed();
    }
}

int main(void) {
    unlock();
    while (1) {
        check_buttons();
        switch(state){
            case 0: // unlocked, programming
                PORTB ^= (PIN5B | PIN6B); // alternately toggles GREEN LED off and YELLOW LED on
                if(combo_presses == 6)
                    lock();
                break;
            case 1: // locked, checking for combo
                if(combo_presses == 6){
                    if(entered_combo == stored_combo)
                        unlock();
                    else
                        incorrect_combo();
                }
                break;
        }
    }
}