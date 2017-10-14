/*
 * File:   main.c
 * Author: ALUNO
 *
 * Created on 23 de Agosto de 2017, 11:29
 */


#include <xc.h>
#include <pic18f4550.h>
#include "config.h"
#include "serial.h"
#include "atraso.h"
#include "adc.h"
#include "pwm.h"

// Se nao houver configuração do tempo primeiro a refeição nunca será dada
// pois o valor é 0xFF

signed char vent_on = 0, aquec_on=0,     // variaveis controladoras da ventoinha e do aquecedor
        hora=0,minuto=0,segundo=0,       // variaveis que guardam o tempo e o avançam 
        hora_refeicao=25, minuto_refeicao=61;// variaveis de hora e minuto da refeição, impossiveis inicialmente
unsigned char gira_servo = 0, temperatura_des = 23; // temperatura atual e temperatura desejada
unsigned int temp,temperatura = 0; // variavel auxiliar 
interrupt void itrp(){
    if(INTCONbits.T0IF == 1){
        INTCONbits.T0IF = 0;
        TMR0L = 0xEE;// TMR0ini= 34286  0x85EE
        TMR0H= 0x85;//
        segundo++;
        if(segundo==60){
            minuto++;
            segundo=0;
        }
        if(minuto==60){
            hora++;
            minuto=0;
        }
        if(hora==24){
            hora=0;
        }
        
        if(hora_refeicao==hora && minuto_refeicao == minuto && segundo == 0){
            //Fazer o processo de dar comida
            gira_servo = 1;
            
        }
        
    }
    
    if(PIR2bits.CCP2IF == 1){
        PIR2bits.CCP2IF = 0;
        PORTCbits.RC1 = ~PORTCbits.RC1;
        if(gira_servo == 1){
            // Aumenta o angulo gradualmente
            temp ++;
            if(temp == 1){
                CCPR2L = 0x9A;// 666  <- servo motor
                CCPR2H = 0x02;
            }
            else if(temp == 1000){
                CCPR2L = 0x40;// 832  <- servo motor
                CCPR2H = 0x03; 
            }
            else if(temp == 2000){
                CCPR2L = 0xE8;// 1000  ~~ +90º <- servo motor
                CCPR2H = 0x03;
            }else if(temp == 2500){
                temp = 0;
                gira_servo = 0;
                CCPR2L = 0xF4;// 500  ~~ -90º <- servo motor
                CCPR2H = 0x01;
            }          
        }
        
    }
    
    if(PIR1bits.TMR1IF == 1){
        PIR1bits.TMR1IF = 0;
        temperatura = adc_amostra(2)/2;
        if(aquec_on == 1){
            if(temperatura < temperatura_des){
                PORTCbits.RC5 = 1;
            }else{
                PORTCbits.RC5 = 0;
            }
            // ajuda do gamboa
        }else{
            PORTCbits.RC5 = 0;
        }
        if(vent_on == 1){
            if(temperatura <= temperatura_des){
                PWM1_Set_Duty(0);
            }else if(temperatura - temperatura_des > 2){
                PWM1_Set_Duty(255);
            }else if(temperatura - temperatura_des < 2 && temperatura - temperatura_des >=0){
                PWM1_Set_Duty(127);
            }
        }else{
            PWM1_Set_Duty(0);
        }
    }
}

void setup(){
    INTCONbits.GIE = 0; // desligando interruptçoes
    TRISCbits.TRISC7 = 1; // RX
    TRISCbits.TRISC1 = 0; // pulse width output para o servo motor 
    TRISAbits.TRISA2 = 1; // LM35, como o simulador dispoe do lm35 nessa porta, coloquei essa no codigo, 
                          // que esta diferente da placa projetada no kicad, mas a placa do kicad esta 
                          // de acordo com a placa da picgenios, a porta está errada no simulador
    PORTCbits.RC5 = 0; // desliga aquecedor
    // pwm pro ventilador
    // interrupção a cada 1 segundo ~~ TMR0 
    T0CONbits.T08BIT = 0;// modo 16 bits
    T0CONbits.PSA = 0;// Prescaler interno
    T0CONbits.T0CS = 0;//Clock source interna
    T0CONbits.T0SE = 0;// Source edge low to high
    T0CONbits.T0PS2 = 1;// Prescaler = 64 
    T0CONbits.T0PS1 = 0;// 
    T0CONbits.T0PS0 = 1;// 
    INTCONbits.T0IE = 1; // lige interrupção do timer 0
    INTCONbits.T0IF = 0;// zera flag
    TMR0L = 0xEE;// TMR0ini= 34286  0x85EE
    TMR0H= 0x85;//
    // 2 ms ~~ TMR3 ~~
    T3CONbits.TMR3CS = 0;
    T3CONbits.T3CKPS = 0b10;
    TMR3L = 0x18 ; // 64536
    TMR3H= 0xFC ;
    // Modulo CCP usando TMR3
    CCP2CON = 0x0B;
    CCPR2L = 0xF4;// 500  ~~ -90º <- servo motor
    CCPR2H = 0x01;
    T3CONbits.T3CCP1= 0; // TMR3 clock source CCP2
    T3CONbits.T3CCP2 = 1;
    // ~~~ TMR1 ~~ 0,52s
    T1CONbits.TMR1CS = 0;
    T1CONbits.T1CKPS = 0b11; // 1:16 Prescaler
    TMR1L = 0x00; 
    TMR1H = 0x00;
    PIE1bits.TMR1IE = 1;
    PIR1bits.TMR1IF = 0;
    // ~~~~~~~
    PIE2bits.CCP2IE = 1;
    PIR2bits.CCP2IF = 0;
    T0CONbits.TMR0ON = 1; // Liga
    T1CONbits.TMR1ON = 1;// Liga
    T3CONbits.TMR3ON = 1; // liga
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;// liga interrupção
    serial_init();
    PWM1_Init(5000);
    PWM1_Set_Duty(0);
    PWM1_Start();
    adc_init();
}
unsigned char buffer[5];
void loop(){
    serial_rx_str_until(buffer,5,'f');
    if(buffer[0] == 'i'){
        // inicio | primeira configuração e sincronização do tempo. 
        hora = buffer[1];
        minuto = buffer[2];
        segundo = buffer[3];
        
    }else if(buffer[0] == 'r'){
        // configura refeiçao
        hora_refeicao = buffer[1];
        minuto_refeicao = buffer[2];       
        // nao usar precisão de segundos pois so irá pesar desnecessariamente
    }else if(buffer[0] == 't'){
        // envia temperatura e horario que esta definido como desejado
            
        temperatura = adc_amostra(2)/2;
        atraso_ms(20);
        serial_tx(temperatura_des);
        atraso_ms(100);
        serial_tx(hora_refeicao);
        atraso_ms(100);
        serial_tx(minuto_refeicao);
        atraso_ms(100);
        serial_tx(temperatura);
        atraso_ms(100);
    }else if(buffer[0] == 'e'){
        temperatura_des = buffer[1];
    }else if(buffer[0] == 'v'){        
        // ventilador ligado ou desligado
        vent_on = buffer[1]; 
    }else if(buffer[0] == 'a'){// aquecedor ligado ou desligado
        aquec_on = buffer[1];
    }
}

