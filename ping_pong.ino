#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <avr/interrupt.h>

//LIGACOES (CONFERIDAS NO TINKERCAD)
#define LED_1 PD4 //LED VERDE (JOGADOR 1)
#define LED_2 PD5 //LED AMARELO (JOGADOR 2)
#define BOTAO_1 PD2 //INT0
#define BOTAO_2 PD3 //INT1
#define POTENCIOMETRO PC0 //A0

//LCD 16x2 NO MODO 4 BITS: D4-D7 NOS PINOS 8-11 (PB0-PB3)
#define LCD_RS PB4 //PINO 12
#define LCD_E PB5 //PINO 13
#define LCD_DADOS 0x0F //MASCARA DE PB0 A PB3

#define PONTUACAO_LIMITE 10
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

//ESPERA PELO TIMER (SEM _delay_ms). COMO O CONTADOR E DE 1ms,
//ESPERAR "ms" GARANTE PELO MENOS ms-1 MILISSEGUNDOS
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

//---------------- LCD ----------------
//MANDA 4 BITS PRO LCD E DA UM PULSO NO ENABLE
void lcdEnviaNibble(uint8_t nibble){
  PORTB = (PORTB & ~LCD_DADOS) | (nibble & LCD_DADOS);
  PORTB |= (1 << LCD_E);
  esperaMs(2);
  PORTB &= ~(1 << LCD_E);
  esperaMs(2);
}

//MANDA 1 BYTE EM DUAS PARTES: PRIMEIRO OS 4 BITS DE CIMA, DEPOIS OS DE BAIXO
//rs = 0 -> COMANDO, rs = 1 -> CARACTERE
void lcdEnviaByte(uint8_t valor, uint8_t rs){
  if(rs) PORTB |= (1 << LCD_RS);
  else PORTB &= ~(1 << LCD_RS);
  lcdEnviaNibble(valor >> 4);
  lcdEnviaNibble(valor & 0x0F);
}

void lcdComando(uint8_t comando){
  lcdEnviaByte(comando, 0);
}

void lcdEscreveChar(char c){
  lcdEnviaByte(c, 1);
}

void lcdEscreveTexto(const char *texto){
  while(*texto) lcdEscreveChar(*texto++);
}

//ESCREVE UM NUMERO SEMPRE COM 2 DIGITOS (EX: 07)
void lcdEscreve2Digitos(uint8_t n){
  lcdEscreveChar('0' + (n / 10) % 10);
  lcdEscreveChar('0' + n % 10);
}

//POSICIONA O CURSOR (linha 0 OU 1, coluna 0 A 15)
void lcdCursor(uint8_t linha, uint8_t coluna){
  lcdComando(0x80 | ((linha ? 0x40 : 0x00) + coluna));
}

void lcdLimpa(){
  lcdComando(0x01);
  esperaMs(3); //O COMANDO DE LIMPAR DEMORA MAIS (1,52ms)
}

//SEQUENCIA DE INICIALIZACAO DO LCD (HD44780) NO MODO 4 BITS,
//COM OS TEMPOS DO DATASHEET
void configuracaoLCD(){
  DDRB |= LCD_DADOS | (1 << LCD_RS) | (1 << LCD_E); //PINOS DO LCD COMO SAIDA
  PORTB &= ~(LCD_DADOS | (1 << LCD_RS) | (1 << LCD_E)); //RS = 0 -> COMANDOS
  esperaMs(100); //ESPERA O LCD LIGAR (MINIMO 40ms)

  lcdEnviaNibble(0x03);
  esperaMs(6); //MINIMO 4,1ms
  lcdEnviaNibble(0x03);
  esperaMs(2);
  lcdEnviaNibble(0x03);
  esperaMs(2);
  lcdEnviaNibble(0x02); //ENTRA NO MODO 4 BITS
  esperaMs(2);

  lcdComando(0x28); //4 BITS, 2 LINHAS, FONTE 5x8
  lcdComando(0x0C); //DISPLAY LIGADO, SEM CURSOR
  lcdComando(0x06); //CURSOR ANDA PRA DIREITA
  lcdLimpa();
}

//TELA DE CONFIGURACAO: DESENHA A TELA INTEIRA UMA VEZ SO
void telaConfiguracao(uint8_t pontuacaoMaxima){
  lcdLimpa();
  lcdEscreveTexto("PLACAR PINGPONG");
  lcdCursor(1, 0);
  lcdEscreveTexto("Ate ");
  lcdEscreve2Digitos(pontuacaoMaxima);
  lcdEscreveTexto(" pontos");
}

//QUANDO O POTENCIOMETRO MUDA, REESCREVE SO O NUMERO (SEM LIMPAR A TELA, NAO PISCA)
void telaAtualizaMaxima(uint8_t pontuacaoMaxima){
  lcdCursor(1, 4);
  lcdEscreve2Digitos(pontuacaoMaxima);
}

//TELA DO JOGO: "J1 05 x 03 J2" NA LINHA 1, SAQUE E TEMPO NA LINHA 2
void telaPlacar(uint8_t p1, uint8_t p2, uint8_t sacador){
  lcdCursor(0, 0);
  lcdEscreveTexto("J1 ");
  lcdEscreve2Digitos(p1);
  lcdEscreveTexto(" x ");
  lcdEscreve2Digitos(p2);
  lcdEscreveTexto(" J2  ");
  lcdCursor(1, 0);
  lcdEscreveTexto("Saque:J");
  lcdEscreveChar('0' + sacador);
}

//ATUALIZA SO O TEMPO DE PARTIDA (mm:ss) NO CANTO DA LINHA 2
void telaTempo(uint32_t segundos){
  lcdCursor(1, 11);
  lcdEscreve2Digitos(segundos / 60);
  lcdEscreveChar(':');
  lcdEscreve2Digitos(segundos % 60);
}

void telaFimDeJogo(uint8_t vencedor, uint8_t p1, uint8_t p2){
  lcdLimpa();
  lcdEscreveTexto("VENCEDOR: J");
  lcdEscreveChar('0' + vencedor);
  lcdCursor(1, 0);
  lcdEscreveTexto("Final: ");
  lcdEscreve2Digitos(p1);
  lcdEscreveTexto(" x ");
  lcdEscreve2Digitos(p2);
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

//DESCOBRE QUEM SACA: TROCA A CADA 2 PONTOS, NAO IMPORTA QUEM FEZ.
//0-1 PONTOS: J1 | 2-3 PONTOS: J2 | 4-5 PONTOS: J1 ...
uint8_t calculaSacador(uint8_t p1, uint8_t p2){
  uint8_t total = p1 + p2;
  return ((total / 2) % 2 == 0) ? 1 : 2;
}

//PISCA O LED DO VENCEDOR QUANDO O JOGO ACABA
void sinalizaFimDeJogo(uint8_t vencedor){
  uint8_t led = (vencedor == 1) ? (1 << LED_1) : (1 << LED_2);
  PORTD &= ~((1 << LED_1) | (1 << LED_2));
  for(uint8_t i = 0; i < 5; i++){
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

//FAZ A MEDIA DE 16 LEITURAS PRA DIMINUIR O RUIDO DO POTENCIOMETRO
uint16_t leituraADCMedia(){
  uint32_t soma = 0;
  for(uint8_t i = 0; i < 16; i++) soma += leituraADC();
  return soma / 16;
}

//TRANSFORMA A LEITURA DO POTENCIOMETRO (0 A 1023) EM UMA PONTUACAO DE 1 A 10.
//HISTERESE: SO ACEITA UMA LEITURA NOVA SE ELA MUDAR MAIS DE HISTERESE_ADC
//EM RELACAO A ULTIMA ACEITA. ASSIM O VALOR NAO FICA PULANDO ENTRE DOIS NUMEROS.
#define HISTERESE_ADC 25
uint16_t adcAceito = 0xFFFF; //0xFFFF = AINDA NAO LEU NADA

uint8_t lerPontuacaoMaxima(){
  uint16_t leitura = leituraADCMedia();
  int16_t diferenca = (int16_t)leitura - (int16_t)adcAceito;
  if(adcAceito == 0xFFFF || diferenca > HISTERESE_ADC || diferenca < -HISTERESE_ADC){
    adcAceito = leitura;
  }
  return 1 + ((uint32_t)adcAceito * PONTUACAO_LIMITE) / 1024;
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
  PORTD &= ~((1 << BOTAO_1) | (1 << BOTAO_2)); //SEM PULL-UP INTERNO (TEM PULL-DOWN EXTERNO)
  DDRC &= ~(1 << POTENCIOMETRO); //POTENCIOMETRO COMO ENTRADA

  configuracaoTimer();
  configuracaoInterrupcao();
  configuracaoADC();
  configuracaoUART();
  sei();

  //MENSAGENS DE DIAGNOSTICO: SE A SEGUNDA NAO APARECER, O PROGRAMA
  //TRAVOU NO esperaMs() (OU SEJA, O TIMER NAO ESTA FUNCIONANDO)
  uartEnviaTexto("\r\nIniciando LCD...\r\n");
  configuracaoLCD(); //DEPOIS DO sei() PORQUE O LCD USA O esperaMs()
  uartEnviaTexto("LCD pronto\r\n");

  Estado estado = CONFIGURACAO;
  uint8_t p1 = 0, p2 = 0;
  uint8_t pontuacaoMaxima = 0;
  uint8_t vencedor = 0;
  uint32_t inicioPartida = 0;
  uint32_t ultimoSegundo = 0;

  uartEnviaTexto("\r\n=== PLACAR DE PING-PONG ===\r\n");
  acendeSacador(1);

  while(1){
    switch(estado){

      //ANTES DO PRIMEIRO PONTO DA PRA AJUSTAR A PONTUACAO NO POTENCIOMETRO
      case CONFIGURACAO: {
        uint8_t novaMaxima = lerPontuacaoMaxima();
        if(novaMaxima != pontuacaoMaxima){
          //pontuacaoMaxima == 0 -> ACABOU DE ENTRAR NA CONFIGURACAO: DESENHA A TELA TODA
          if(pontuacaoMaxima == 0) telaConfiguracao(novaMaxima);
          else telaAtualizaMaxima(novaMaxima);
          pontuacaoMaxima = novaMaxima;
          uartEnviaTexto("Pontuacao da partida: ");
          uartEnviaNumero(pontuacaoMaxima);
          uartEnviaTexto("\r\n");
        }
        //O PRIMEIRO PONTO COMECA O JOGO
        if(pontoPendente1 || pontoPendente2){
          uartEnviaTexto("Partida iniciada!\r\n");
          inicioPartida = lerTempo();
          ultimoSegundo = 0;
          lcdLimpa();
          telaPlacar(0, 0, 1);
          telaTempo(0);
          estado = JOGANDO;
        }
        break;
      }

      case JOGANDO: {
        //UM PONTO POR VEZ: SE OS DOIS APERTAREM JUNTO, O OUTRO FICA PRA PROXIMA VOLTA
        uint8_t marcou = 0;
        if(pegaPonto(&pontoPendente1)){ p1++; marcou = 1; }
        else if(pegaPonto(&pontoPendente2)){ p2++; marcou = 2; }

        uint32_t segundos = (lerTempo() - inicioPartida) / 1000;

        if(marcou){
          //LOG DE CADA PONTO NO TERMINAL SERIAL
          uartEnviaTexto("[");
          uartEnviaNumero(segundos);
          uartEnviaTexto("s] Ponto do jogador ");
          uartEnviaNumero(marcou);
          uartEnviaTexto(" | Placar: ");
          uartEnviaPlacar(p1, p2);
          uartEnviaTexto("\r\n");

          //ACABA QUANDO ALGUEM CHEGA NA PONTUACAO MAXIMA
          if(p1 >= pontuacaoMaxima){
            vencedor = 1;
            estado = FIM_DE_JOGO;
          } else if(p2 >= pontuacaoMaxima){
            vencedor = 2;
            estado = FIM_DE_JOGO;
          } else {
            uint8_t sacador = calculaSacador(p1, p2);
            acendeSacador(sacador);
            telaPlacar(p1, p2, sacador);
          }
        }

        //ATUALIZA O RELOGIO DA TELA UMA VEZ POR SEGUNDO
        if(estado == JOGANDO && segundos != ultimoSegundo){
          ultimoSegundo = segundos;
          telaTempo(segundos);
        }
        break;
      }

      case FIM_DE_JOGO: {
        uartEnviaTexto("FIM DE JOGO! Vencedor: jogador ");
        uartEnviaNumero(vencedor);
        uartEnviaTexto(" | Placar final: ");
        uartEnviaPlacar(p1, p2);
        uartEnviaTexto("\r\n\r\n");

        telaFimDeJogo(vencedor, p1, p2);
        sinalizaFimDeJogo(vencedor); //PISCA 5 VEZES (3 SEGUNDOS) COM O RESULTADO NA TELA

        //ZERA TUDO PRO JOGO NOVO (IGNORA TOQUES FEITOS NO FIM DE JOGO)
        p1 = 0;
        p2 = 0;
        pegaPonto(&pontoPendente1);
        pegaPonto(&pontoPendente2);
        pontuacaoMaxima = 0; //FORCA MOSTRAR A TELA DE CONFIGURACAO DE NOVO
        acendeSacador(1); //JOGADOR 1 SACA NO JOGO NOVO
        estado = CONFIGURACAO;
        break;
      }
    }
  }
}
