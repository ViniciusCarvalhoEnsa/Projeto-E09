#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <avr/interrupt.h>

#define LED_1 PD4
#define LED_2 PD5
#define BOTAO_1 PD2
#define BOTAO_2 PD3
#define POTENCIOMETRO PC0

#define PONTUACAO_LIMITE 30
#define DEBOUNCE_MS 50

#define BAUD 9600
#define UBRR_VALOR ((F_CPU / 16 / BAUD) - 1)

//MAQUINA DE ESTADOS DO JOGO
typedef enum { CONFIGURACAO, JOGANDO, FIM_DE_JOGO } Estado;

//VOLATILE PORQUE AS INTERRUPCOES MEXEM NESSAS VARIAVEIS
volatile uint32_t tempo = 0;
volatile uint32_t ultimaMudanca1 = 0, ultimaMudanca2 = 0;
volatile uint8_t pontoPendente1 = 0, pontoPendente2 = 0;

//CONTA O TEMPO EM MS
ISR(TIMER0_COMPA_vect){
  tempo++;
}

//BOTAO 1: A INTERRUPCAO DISPARA EM QUALQUER MUDANCA DO PINO.
//SO CONTA O PONTO SE O BOTAO FOI APERTADO (PINO EM NIVEL ALTO) E O PINO
//ESTAVA PARADO HA MAIS DE DEBOUNCE_MS. ASSIM A TREPIDACAO DO BOTAO,
//TANTO AO APERTAR QUANTO AO SOLTAR, NAO GERA PONTO FALSO.
ISR(INT0_vect){
  uint8_t apertado = PIND & (1 << BOTAO_1);
  uint32_t estavel = tempo - ultimaMudanca1;
  ultimaMudanca1 = tempo;
  if(apertado && estavel >= DEBOUNCE_MS) pontoPendente1 = 1;
}

//BOTAO 2: MESMA LOGICA DO BOTAO 1
ISR(INT1_vect){
  uint8_t apertado = PIND & (1 << BOTAO_2);
  uint32_t estavel = tempo - ultimaMudanca2;
  ultimaMudanca2 = tempo;
  if(apertado && estavel >= DEBOUNCE_MS) pontoPendente2 = 1;
}

//PEGA O TEMPO COM AS INTERRUPCOES DESLIGADAS PRA NAO LER ERRADO
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

//---------------- UART ----------------
void configuracaoUART(){
  UBRR0H = (uint8_t)(UBRR_VALOR >> 8);
  UBRR0L = (uint8_t)UBRR_VALOR;
  UCSR0B = (1 << TXEN0); //LIGA O TRANSMISSOR
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //8 BITS, SEM PARIDADE, 1 STOP BIT
}

void uartEnviaChar(char c){
  while(!(UCSR0A & (1 << UDRE0))); //ESPERA O BUFFER FICAR LIVRE
  UDR0 = c;
}

void uartEnviaTexto(const char *texto){
  while(*texto) uartEnviaChar(*texto++);
}

void uartEnviaNumero(uint32_t n){
  char buffer[11];
  uint8_t i = 0;
  do {
    buffer[i++] = '0' + (n % 10);
    n /= 10;
  } while(n > 0);
  while(i > 0) uartEnviaChar(buffer[--i]);
}

//MANDA O PLACAR NO FORMATO "3 x 2"
void uartEnviaPlacar(uint8_t p1, uint8_t p2){
  uartEnviaNumero(p1);
  uartEnviaTexto(" x ");
  uartEnviaNumero(p2);
}

//---------------- LEDS ----------------
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

//TROCA O SACADOR A CADA 2 PONTOS. NO DESEMPATE (OS DOIS A 1 PONTO
//DE FECHAR O JOGO) O SAQUE TROCA A CADA PONTO, COMO NA REGRA OFICIAL.
void alternaSacador(uint8_t p1, uint8_t p2, uint8_t pontuacaoMaxima){
  uint8_t total = p1 + p2;
  uint8_t desempate = (p1 >= pontuacaoMaxima - 1) && (p2 >= pontuacaoMaxima - 1);
  uint8_t trocas = desempate ? total : total / 2;
  acendeSacador((trocas % 2 == 0) ? 1 : 2);
}

//PISCA O LED DO VENCEDOR QUANDO O JOGO ACABA
void sinalizaFimDeJogo(uint8_t vencedor){
  uint8_t led = (vencedor == 1) ? (1 << LED_1) : (1 << LED_2);
  PORTD &= ~((1 << LED_1) | (1 << LED_2));
  for(uint8_t i = 0; i < 3; i++){
    PORTD |= led;
    esperaMs(300);
    PORTD &= ~led;
    esperaMs(300);
  }
}

//---------------- CONFIGURACOES ----------------
void configuracaoTimer(){
  TCCR0A = (1 << WGM01); //MODO CTC
  TCCR0B = (1 << CS01) | (1 << CS00); //PRESCALER 64
  OCR0A = 249; //16MHz / 64 / 250 = 1ms
  TIMSK0 = (1 << OCIE0A); //INTERRUPCAO DO CANAL A
}

void configuracaoInterrupcao(){
  EICRA = (1 << ISC00) | (1 << ISC10); //QUALQUER MUDANCA NO INT0 E NO INT1
  EIMSK = (1 << INT0) | (1 << INT1); //LIGA INT0 E INT1
}

void configuracaoADC(){
  ADMUX = (1 << REFS0); //VREF = AVCC (5V), LEITURA NO A0
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //LIGA O ADC COM PRESCALER 128
}

uint16_t leituraADC(){
  ADCSRA |= (1 << ADSC); //INICIA CONVERSAO
  while(ADCSRA & (1 << ADSC)); //ESPERA A CONVERSAO
  uint16_t leitura = ADCL; //PRIMEIRO O ADCL, DEPOIS O ADCH
  leitura |= (ADCH << 8);
  return leitura;
}

//TRANSFORMA A LEITURA DO POTENCIOMETRO EM UMA PONTUACAO DE 1 A 30
uint8_t lerPontuacaoMaxima(){
  return 1 + ((uint32_t)leituraADC() * (PONTUACAO_LIMITE - 1)) / 1023;
}

//PEGA E LIMPA A FLAG DE PONTO COM AS INTERRUPCOES DESLIGADAS
uint8_t pegaPonto(volatile uint8_t *flag){
  cli();
  uint8_t valor = *flag;
  *flag = 0;
  sei();
  return valor;
}

int main(void){
  DDRD |= (1 << LED_1) | (1 << LED_2); //LEDS COMO SAIDA
  DDRD &= ~((1 << BOTAO_1) | (1 << BOTAO_2)); //BOTOES COMO ENTRADA
  PORTD &= ~((1 << BOTAO_1) | (1 << BOTAO_2)); //SEM PULL-UP INTERNO (PRECISA DE PULL-DOWN EXTERNO)
  DDRC &= ~(1 << POTENCIOMETRO); //POTENCIOMETRO COMO ENTRADA

  configuracaoTimer();
  configuracaoInterrupcao();
  configuracaoADC();
  configuracaoUART();
  sei();

  Estado estado = CONFIGURACAO;
  uint8_t p1 = 0, p2 = 0;
  uint8_t pontuacaoMaxima = 0;
  uint8_t vencedor = 0;

  uartEnviaTexto("\r\n=== PLACAR DE PING-PONG ===\r\n");
  acendeSacador(1);

  while(1){
    switch(estado){

      //ANTES DO PRIMEIRO PONTO DA PRA AJUSTAR A PONTUACAO NO POTENCIOMETRO
      case CONFIGURACAO: {
        uint8_t novaMaxima = lerPontuacaoMaxima();
        if(novaMaxima != pontuacaoMaxima){
          pontuacaoMaxima = novaMaxima;
          uartEnviaTexto("Pontuacao da partida: ");
          uartEnviaNumero(pontuacaoMaxima);
          uartEnviaTexto("\r\n");
        }
        //O PRIMEIRO PONTO COMECA O JOGO
        if(pontoPendente1 || pontoPendente2){
          uartEnviaTexto("Partida iniciada!\r\n");
          estado = JOGANDO;
        }
        break;
      }

      case JOGANDO: {
        uint8_t marcou = 0;
        if(pegaPonto(&pontoPendente1)){ p1++; marcou = 1; }
        if(pegaPonto(&pontoPendente2)){ p2++; marcou = 2; }

        if(marcou){
          //LOG DE CADA PONTO NO TERMINAL SERIAL
          uartEnviaTexto("[");
          uartEnviaNumero(lerTempo() / 1000);
          uartEnviaTexto("s] Ponto do jogador ");
          uartEnviaNumero(marcou);
          uartEnviaTexto(" | Placar: ");
          uartEnviaPlacar(p1, p2);
          uartEnviaTexto("\r\n");

          //ACABA QUANDO ALGUEM CHEGA NA PONTUACAO COM 2 PONTOS DE VANTAGEM
          if(p1 >= pontuacaoMaxima && p1 >= p2 + 2){
            vencedor = 1;
            estado = FIM_DE_JOGO;
          } else if(p2 >= pontuacaoMaxima && p2 >= p1 + 2){
            vencedor = 2;
            estado = FIM_DE_JOGO;
          } else {
            alternaSacador(p1, p2, pontuacaoMaxima);
          }
        }
        break;
      }

      case FIM_DE_JOGO: {
        uartEnviaTexto("FIM DE JOGO! Vencedor: jogador ");
        uartEnviaNumero(vencedor);
        uartEnviaTexto(" | Placar final: ");
        uartEnviaPlacar(p1, p2);
        uartEnviaTexto("\r\n\r\n");

        sinalizaFimDeJogo(vencedor);

        //ZERA TUDO PRO JOGO NOVO (IGNORA TOQUES FEITOS DURANTE O PISCA-PISCA)
        p1 = 0;
        p2 = 0;
        pegaPonto(&pontoPendente1);
        pegaPonto(&pontoPendente2);
        pontuacaoMaxima = 0; //FORCA MOSTRAR A PONTUACAO DE NOVO
        acendeSacador(1); //JOGADOR 1 SACA NO JOGO NOVO
        estado = CONFIGURACAO;
        break;
      }
    }
  }
}
