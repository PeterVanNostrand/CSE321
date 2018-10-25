#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/iotn85.h>

// Location of the bit in DDRB, PORTB, and PINB associated 
// with the corresponding pin. Used for masking pin values
#define PIN2B (1 << PB3)
#define PIN3B (1 << PB4)
#define PIN5B (1 << PB0)
#define PIN6B (1<<PB1)
#define PIN7B (1<<PB2)

enum State{
    // Safe is unlocked and ready to receive a new combo. When a button is pressed the safe will move to the UNLOCKED_BUTTON_PRESSED state.
    // GREEN and YELLOW LEDs are on. 
    UNLOCKED = 0,
    // Safe is unlocked and a button has just been pressed. Button interrupt is disabled to prevent accidental keypress (functions as debouncing)
    // If the button pressed was the 6th combo digit, the combo will be stored and the safe will move to the LOCKED state.  
    // Otherwise it will store the digit and return to the UNLOCKED state.  GREEN, YELLOW, and BLUE LEDs are on 
    UNLOCKED_BUTTON_PRESSED = 1,
    // Safe is locked and ready to recieve a combo.  When a button is pressed the safe will move to the LOCKED_BUTTON_PRESSED state
    // RED and BLUE LEDs are on
    LOCKED = 2,
    // Safe is locked and a button has just been pressed. If the button pressed was the 6th code digit, and the code entered matches that previously entered
    // the safe will move to the unlocked state.  If the code does not match the previously entered one it will move to the LOCKED_INVALID_CODE state.
    // RED LED is on
    LOCKED_BUTTON_PRESSED = 3,
    // Safe is locked and the user has entered an incorrect combo.  The entered combo is cleared and the safe returns to the LOCKED state
    // YELLOW and RED LEDs are on
    LOCKED_INVALID_CODE = 4
};

volatile enum State current_state;
volatile uint8_t entered_combo;
volatile uint8_t stored_combo;
volatile uint8_t combo_presses;

void lock(){
    // save the entered combo
    stored_combo = entered_combo;
    // reset temporary fields to recieve first unlock attempt
    entered_combo = 0;
    combo_presses = 0;
    //set the state to locked and turn on the RED LED
    current_state = LOCKED;
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
    current_state = UNLOCKED;
    DDRB = PIN5B | PIN6B;
    PORTB = PIN5B;
}

void incorrect_combo(){
    entered_combo = 0;
    combo_presses = 0;
    current_state = LOCKED_INVALID_CODE;
}

/*
 * Pin change interrupt handler.  This function is called automatically when one of the buttons is pressed. It determines which button was pressed
 * and appends the appropriate digit to the entered combo.  If 6 combo digits have been entered it checks the combo if neccessary and then switches
 * the state from UNLOCKED-->LOCKED or LOCKED-->UNLOCKED.  The interrupt is then disabled for a short time, and reenabled by a timer.  This prevents
 * button bouncing from causing the button press to register multiple times 
 */
ISR(PCINT0_vect)
{
    if(!(PINB&PIN2B)){ // button 1 is pressed
        // append a 0 to the entered combo
        entered_combo <<= 1;
        combo_presses++;
        current_state += 1;
    }
    else if(!(PINB&PIN3B)){ // button 2 is pressed
        // append a 1 to the entered combo
        entered_combo <<= 1;
        entered_combo |= 1;
        combo_presses++;
        current_state += 1;
    }
    if(combo_presses == 6){ //if the user finished entering a combination
        if(current_state == UNLOCKED || current_state == UNLOCKED_BUTTON_PRESSED) // and the safe was unlocked
            lock();
        else if(current_state == LOCKED || current_state == LOCKED_BUTTON_PRESSED){ // and the safe was locked
            if(entered_combo==stored_combo) // check the combination
                unlock(); // if combo correct
            else
                incorrect_combo(); //if combo incorrect
        }
    }
    PCMSK = 0; // disable pin change interrupts to prevent button bouncing recalling this ISR
    TCCR0B = 0b101; // set timer 0 prescale, starts timer which will renable this ISR
    GIFR = 0; // clear any pin change interrupts that have occured while we ran this code
}

/*
 * Timer 0 interrupt handler.  This fuction is called automatically when timer 0 hits its match value.  By this time the bounce effects from a button press has settled
 * so the pin change interrupt handler is renabled and the state returns to its value from before a button was pressed [STATE]_BUTTON_PRESSED --> [STATE]
*/
ISR(TIMER0_COMPA_vect) {
    PCMSK |= (PIN3B | PIN2B); // pin change interrupt enabled for pins 2 and 3
    TCCR0B = 0b000; //set timer 0 prescale to 0 to stop this timer as it is no longer needed
    switch (current_state){
        case UNLOCKED_BUTTON_PRESSED: // switch back to UNLOCKED state after button press
            current_state = UNLOCKED;
            PORTB = 0;
            DDRB = PIN5B | PIN6B;
            PORTB = PIN5B;
            break;
        case LOCKED_BUTTON_PRESSED: // switch back to LOCKED state after button press
            current_state = LOCKED;
            PORTB = 0;
            DDRB = PIN6B | PIN7B;
            PORTB = PIN7B;
            break;
        case LOCKED_INVALID_CODE: // switch back to LOCKED state after incorrect combo
            current_state = LOCKED;
            PORTB = 0;
            DDRB = PIN6B | PIN7B;
            PORTB = PIN7B;
            break;
    }
}

/*
 * Timer 1 interrupt handler.  This function is set to be called periodically to update the status of UI LEDs. Based upon the current state it switches LEDs on/off
 * quickly to make them appear as if they are on at the same time
 */
ISR(TIMER1_COMPA_vect)
{
    switch(current_state){
        case UNLOCKED: // safe is unlocked, switch between GREEN and YELLOW LEDs
            DDRB = PIN5B | PIN6B;
            PORTB ^= PIN5B | PIN6B;
            break;
        case UNLOCKED_BUTTON_PRESSED: // safe is unlocked and a button is pressed.  Switch between GREEN, YELLOW, and BLUE LEDs
            if(DDRB & PIN7B){ // BLUE LED was on, switch to GREEN LED
                PORTB = 0;
                DDRB = PIN5B | PIN6B;
                PORTB  = PIN5B;
            }
            else if (PORTB & PIN5B){ // GREEN LED was on, switch to YELLOW LED
                DDRB = PIN5B | PIN6B;
                PORTB ^= PIN5B | PIN6B; 
            }
            else if(PORTB & PIN6B){ // YELLOW LED was on, switch to BLUE LED
               PORTB = 0;
               DDRB = PIN6B | PIN7B;
                PORTB = PIN6B;
            }            
            break;
        case LOCKED: // safe is locked, RED LED is already on. Do nothing.
            break;
        case LOCKED_BUTTON_PRESSED: // safe is locked and a button is pressed.  Switch between RED and BLUE LEDs
            DDRB = PIN6B | PIN7B;
            PORTB ^= (PIN6B | PIN7B);
            break;
        case LOCKED_INVALID_CODE: // safe is locked and an incorrect combo was entered.  Switch between RED and YELLOW LEDs
            if(DDRB & PIN7B){ // RED LED was on, switch to YELLOW
                PORTB = 0;
                DDRB = PIN5B | PIN6B;
                PORTB = PIN6B;
            }
            else if(DDRB & PIN6B){ // YELLOW LED was on, switch to RED
                PORTB = 0;
                DDRB = PIN6B | PIN7B;
                PORTB = PIN7B;
            }
            break;
    }
}

int main(void){ 
    current_state = UNLOCKED; // start with the safe unlocked
	DDRB = (PIN5B | PIN6B); // enable pins 6 and 7 as outputs for GREEN and YELLOW LEDs
    PORTB = PIN5B; // set pin 6 high, start with GREEN LED on
	
    // setup interupt for buttons
    GIMSK |= (1 << PCIE); // enable pin change interrupts
	PCMSK |= (PIN3B | PIN2B); // turn on interrupts for pins 2 and 3

    // setup timer 1 to regularly update the LEDs
    TCCR1 |= (1 << CTC1);  // clear timer 1 on compare match
    TCCR1 |= (1 << CS13) | (1 << CS11) | (1 << CS10); // timer 1 clock prescaler 2048, updates every ~2ms
    OCR1C = 1; // timer 1 compare match value 

    // setup timer 0 to renable pin change interrupts
    TCCR0A |= (1<<WGM01); // clear timer 0 on compare match
    TCCR0B = 0b000; //timer 0 stopped
    OCR0A = 255; // timer 0 match value = 10

    // enable timer intterupts
    TIMSK |= (1 << OCIE0A) | (1 << OCIE1A);

    // set default values to 0
    entered_combo = 0;
    stored_combo = 0;
    combo_presses = 0;

    // enable global interrupts
	sei();  
	
	while(1); // infinite loop
}