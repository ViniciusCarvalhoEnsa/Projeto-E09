# Placar Eletrônico de Ping-Pong

Projeto de laboratório da disciplina **E09 – Microcontroladores**.
Área: **Entretenimento**

Placar eletrônico para partidas de tênis de mesa feito com o **ATmega328p** (Arduino Uno).
Cada jogador tem um botão para marcar ponto. O LCD mostra o placar e o tempo de partida, um LED indica quem está sacando e cada ponto é registrado no terminal serial.

---

## Funcionamento

1. **Configuração:** ao ligar, o LCD mostra `PLACAR PINGPONG` e `Ate XX pontos`. Gire o potenciômetro para escolher a pontuação da partida, de **1 a 10**.
2. **Início:** a partida começa no primeiro ponto marcado. A partir daí, a pontuação fica travada.
3. **Durante o jogo:**
   - Cada jogador aperta o **seu botão** quando faz um ponto.
   - O LCD mostra o placar (`J1 03 x 02 J2`), quem está sacando e o tempo de partida (`mm:ss`).
   - O **LED de quem está sacando** fica aceso. O saque troca **a cada 2 pontos**, não importa quem marcou.
4. **Fim de jogo:** quando um jogador chega na pontuação escolhida, o **LED do vencedor pisca** e o LCD mostra o vencedor e o placar final. Depois o sistema volta para a configuração.

### Log no terminal serial

Cada ponto é enviado pela UART (**9600 baud**):

```
=== PLACAR DE PING-PONG ===
Pontuacao da partida: 5
Partida iniciada!
[3s] Ponto do jogador 1 | Placar: 1 x 0
[9s] Ponto do jogador 2 | Placar: 1 x 1
...
FIM DE JOGO! Vencedor: jogador 1 | Placar final: 5 x 3
```

---

## Componentes

| Qtd | Componente |
|---|---|
| 1 | Arduino Uno (ATmega328p) |
| 1 | Display LCD 16x2 (HD44780 / JHD-162A) |
| 2 | Push-buttons (um por jogador) |
| 2 | LEDs (verde = J1, amarelo = J2) |
| 1 | Potenciômetro (pontuação da partida) |
| 1 | Potenciômetro de 10 kΩ (contraste do LCD), opcional mas recomendado |
| 2 | Resistores de pull-down para os botões (10 kΩ recomendado) |
| 2 | Resistores de 330 Ω para os LEDs |
| 1 | Resistor de 220 Ω para o backlight do LCD |
| – | Protoboard e jumpers |

---

## Ligações

### Botões, LEDs e potenciômetro

| Componente | Pino do Arduino | Observação |
|---|---|---|
| Botão J1 | D2 (INT0) | Outro lado no 5V e pull-down para o GND |
| Botão J2 | D3 (INT1) | Outro lado no 5V e pull-down para o GND |
| LED J1 (verde) | D4 | Catodo → resistor 330 Ω → **GND** |
| LED J2 (amarelo) | D5 | Catodo → resistor 330 Ω → **GND** |
| Potenciômetro (pontuação) | A0 | Pino do meio no A0 e as pontas no 5V e no GND |

### LCD 16x2 (modo 4 bits)

| Pino do LCD | Ligação |
|---|---|
| 1 – VSS | GND |
| 2 – VDD | 5V |
| 3 – VO | Meio do potenciômetro de contraste (ou GND) |
| 4 – RS | D12 |
| 5 – RW | GND |
| 6 – E | D13 |
| 7 a 10 – D0 a D3 | Sem ligação |
| 11 – D4 | D8 |
| 12 – D5 | D9 |
| 13 – D6 | D10 |
| 14 – D7 | D11 |
| 15 – A | 5V (com resistor de 220 Ω) |
| 16 – K | GND |

---

## Como usar

1. Monte o circuito conforme as tabelas acima.
2. Abra o `ping_pong.ino` na IDE do Arduino, selecione a placa **Arduino Uno** e faça o upload.
3. Abra o **Monitor Serial** em **9600 baud** para ver o log da partida.
4. Gire o potenciômetro para escolher a pontuação e comece a jogar.

---

## Requisitos da disciplina atendidos

| Requisito | Como foi atendido |
|---|---|
| ATmega328p | Arduino Uno, no Tinkercad e na montagem física |
| 4 periféricos ou mais | Botões, LEDs, potenciômetro (ADC) e LCD 16x2 |
| Máquina de estados | `CONFIGURACAO → JOGANDO → FIM_DE_JOGO` |
| GPIO com máscara, sem bibliotecas | Registradores (`DDRx`, `PORTx`, `PINx`), inclusive o driver do LCD |
| Interrupção externa | Botões no INT0 e no INT1 |
| Sem `_delay_ms()` | Timer0 em modo CTC gera a base de tempo de 1 ms |
| Comunicação UART | Log de cada ponto e do resultado final no terminal serial |

---

## Máquina de estados

```
          primeiro ponto            alguém chega no máximo
CONFIGURACAO ──────────────► JOGANDO ──────────────────────► FIM_DE_JOGO
     ▲                                                            │
     └─────────────── após piscar o LED do vencedor ──────────────┘
```

- **CONFIGURACAO:** lê o potenciômetro e mostra a pontuação escolhida.
- **JOGANDO:** conta os pontos, troca o saque, atualiza o LCD e o tempo e envia o log.
- **FIM_DE_JOGO:** mostra o vencedor, pisca o LED dele e zera o placar.

---

## Dicas para resolver problemas

- **LCD aceso, mas sem nada escrito:** ajuste o contraste (pino VO). Teste ligando o VO direto no GND.
- **Pontuação mudando sozinha ou LCD piscando:** mau contato no potenciômetro. Confira se o pino do meio está no A0 e as pontas no 5V e no GND.
- **LED não acende:** o resistor do LED precisa ir para o **GND**, não para o 5V.
- **Pontos contando sozinhos:** falta o resistor de pull-down no botão.
- **Nada no Monitor Serial:** confira se está em 9600 baud.
