#include <avr/io.h>
#include <avr/interrupt.h>

#define LED_1 PD4
#define LED_2 PD5
#define BOTAO_1 PD2
#define BOTAO_2 PD3
#define POTENCIOMETRO PC0

#define PONTUACAO_LIMITE 30
#define DEBOUNCE_MS 200

//VOLATILE PORQUE AS INTERRUPÇÕES MEXEM NESSAS VARIAVEIS
volatile uint32_t tempo = 0;
volatile uint8_t pontuacao1 = 0, pontuacao2 = 0;
volatile uint32_t ultimoToque1 = 0, ultimoToque2 = 0;

//CONTA O TEMPO EM MS
ISR(TIMER0_COMPA_vect){
  tempo++;
}

//PONTO PRO JOGADOR 1
ISR(INT0_vect){
  if(tempo - ultimoToque1 < DEBOUNCE_MS) return; //DEBOUNCE DO BOTÃO
  ultimoToque1 = tempo;
  pontuacao1++;
}

//PONTO PRO JOGADOR 2
ISR(INT1_vect){
  if(tempo - ultimoToque2 < DEBOUNCE_MS) return; //DEBOUNCE DO BOTÃO
  ultimoToque2 = tempo;
  pontuacao2++;
}

//PEGA O TEMPO COM AS INTERRUPÇÕES DESLIGADAS PRA NÃO LER ERRADO
uint32_t lerTempo(){
  cli();
  uint32_t t = tempo;
  sei();
  return t;
}

void esperaMs(uint32_t ms){
  uint32_t inicio = lerTempo();
  while(lerTempo() - inicio < ms);
}

//ACENDE O LED DE QUEM ESTA SACANDO
void acendeSacador(uint8_t jogador){
  if(jogador == 1){
    PORTD |= (1 << LED_1);
    PORTD &= ~(1 << LED_2);
  } else {
    PORTD |= (1 << LED_2);
    PORTD &= ~(1 << LED_1);
  }
}

//TROCA O SACADOR A CADA 2 PONTOS
void alternaSacador(uint8_t p1, uint8_t p2){
  uint8_t total = p1 + p2;
  if((total / 2) % 2 == 0){
    acendeSacador(1);
  } else {
    acendeSacador(2);
  }
}

//PISCA OS 2 LEDS QUANDO O JOGO ACABA
void sinalizaFimDeJogo(){
  for(uint8_t i = 0; i < 3; i++){
    PORTD |= (1 << LED_1) | (1 << LED_2);
    esperaMs(300);
    PORTD &= ~((1 << LED_1) | (1 << LED_2));
    esperaMs(300);
  }
}

void configuracaoTimer(){
  TCCR0A |= (1 << WGM01); //MODO CTC
  TCCR0B |= (1 << CS01) | (1 << CS00); //PRESCALER 64
  OCR0A = 249; //OCR PARA 1ms COM PRESCALER DE 64
  TIMSK0 |= (1 << OCIE0A); //CANAL DE INTERRUPÇÃO A
}

void configuracaoInterrupcao(){
  EICRA |= (1 << ISC00) | (1 << ISC01) | (1 << ISC10) | (1 << ISC11); //BORDA DE SUBIDA NO INT0 (BOTÃO 1) E INT1 (BOTÃO 2)
  EIMSK |= (1 << INT0) | (1 << INT1); //LIGA INT0 E INT1
}

void configuracaoADC(){
  ADMUX |= (1 << REFS0); //VREF = 5V, LEITURA NO A0
  ADCSRA |= (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //LIGA O ADC COM PRESCALER 128
}

uint16_t leituraADC(){
  ADCSRA |= (1 << ADSC); //INICIA CONVERSÃO
  while(ADCSRA & (1 << ADSC)); //ESPERA A CONVERSÃO
  uint16_t leitura = ADCL; //8 BITS MENOS SIGNIFICATIVOS
  leitura |= (ADCH << 8); //2 BITS MAIS SIGNIFICATIVOS
  return leitura;
}

//TRANSFORMA A LEITURA DO POTENCIOMETRO EM UMA PONTUAÇÃO DE 1 A 30
uint8_t lerPontuacaoMaxima(){
  return 1 + ((uint32_t)leituraADC() * (PONTUACAO_LIMITE - 1)) / 1023;
}

int main(void){
  DDRD |= (1 << LED_1) | (1 << LED_2); //LEDS COMO SAIDA
  DDRD &= ~((1 << BOTAO_1) | (1 << BOTAO_2)); //BOTÕES COMO ENTRADA
  PORTD &= ~((1 << BOTAO_1) | (1 << BOTAO_2)); //SEM PULL UP INTERNO
  DDRC &= ~(1 << POTENCIOMETRO); //POTENCIOMETRO COMO ENTRADA

  configuracaoTimer();
  configuracaoInterrupcao();
  configuracaoADC();
  sei();

  //CONFIGURAÇÃO PRE-JOGO
  uint8_t pontuacaoMaxima = lerPontuacaoMaxima();
  acendeSacador(1); //JOGADOR 1 COMEÇA SACANDO

  while(1){
    uint8_t p1 = pontuacao1;
    uint8_t p2 = pontuacao2;

    //SE O JOGO NÃO COMEÇOU AINDA DA PRA MUDAR A PONTUAÇÃO
    if(p1 == 0 && p2 == 0){
      pontuacaoMaxima = lerPontuacaoMaxima();
    }
    //VERIFICA SE O JOGO ACABOU
    else if(p1 >= pontuacaoMaxima || p2 >= pontuacaoMaxima){
      sinalizaFimDeJogo();
      cli();
      pontuacao1 = 0;
      pontuacao2 = 0;
      sei();
      acendeSacador(1); //JOGADOR 1 SACA NO JOGO NOVO
    }
    //VERIFICA SE PRECISA TROCAR O SACADOR
    else{
      alternaSacador(p1, p2);
    }
  }
}