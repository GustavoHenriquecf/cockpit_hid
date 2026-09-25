/*
 * ============================================================================
 *  COCKPIT CASEIRO - CONTROLADOR HID USB (Arduino Pro Micro / ATmega32U4)
 * ============================================================================
 *  Projeto: Painel de controle físico para FSX / MSFS
 *  Biblioteca: Joystick by Matthew Heironimus
 *
 *  REVISÃO 3 - Correções aplicadas:
 *   - Corrigida a ordem invertida de Rudder/Throttle no construtor do
 *     Joystick (bug que fazia o Windows mostrar "Leme" em vez de Throttle)
 *   - Remapeamento de eixos conforme uso real das manetes:
 *       A0 (Potência)         -> Y Axis      (antes ia para Throttle)
 *       A1 (Mistura)          -> X Axis      (sem alteração)
 *       A2 (Passo de Hélice)  -> Z Axis (Accelerator não é traduzido pelo joy.cpl)
 *   - Throttle e Accelerator desabilitados; Z Axis habilitado no lugar
 *   - Tipo do dispositivo: JOYSTICK_TYPE_JOYSTICK
 *   - Recalibração dos potenciômetros com o curso mecânico real medido:
 *     mínimo = 260, máximo = 570 (X e Y confirmados; ajuste o do Z Axis
 *     separadamente se o curso físico da manete de hélice for diferente)
 *
 *  Funcionalidades:
 *   - 15 botões digitais (chaves/botões com GND comum, INPUT_PULLUP)
 *   - 3 eixos analógicos (Y = Potência, X = Mistura, Z Axis = Hélice)
 *   - Calibração de curso limitado dos potenciômetros (map + constrain)
 *   - Filtro de ruído analógico (deadband/histerese)
 *   - Varredura eficiente dos botões via array + State Change Detection
 *   - Debounce baseado em millis()
 * ============================================================================
 */

#include <Joystick.h>

// ============================================================================
// 1. OBJETO JOYSTICK
// ============================================================================
// JOYSTICK_TYPE_JOYSTICK para o Windows reconhecer corretamente como um
// controle de eixos/quadrante, evitando fusão/nomeação incorreta de eixos
// no joy.cpl.
// 15 botões, 0 hat switches. X Axis, Y Axis e Z Axis habilitados;
// todos os demais eixos ficam obrigatoriamente desabilitados.
Joystick_ Joystick(
    JOYSTICK_DEFAULT_REPORT_ID,
    JOYSTICK_TYPE_JOYSTICK,
    15,     // Número de botões
    0,      // Número de hat switches
    true,   // X Axis        -> habilitado (Mistura)
    true,   // Y Axis        -> habilitado (Potência)
    true,   // Z Axis        -> habilitado (Passo de Hélice)
    false,  // Rx Axis       -> desabilitado
    false,  // Ry Axis       -> desabilitado
    false,  // Rz Axis       -> desabilitado
    false,  // Rudder        -> desabilitado
    false,  // Throttle      -> desabilitado
    false,  // Accelerator   -> desabilitado (não é traduzido pelo joy.cpl)
    false,  // Brake         -> desabilitado
    false   // Steering      -> desabilitado
);

// ============================================================================
// 2. MAPEAMENTO DE HARDWARE - ENTRADAS DIGITAIS
// ============================================================================
// Pinos dos 15 botões/chaves. Todos fecham contato com GND (ativo em LOW).
const uint8_t NUM_BOTOES = 15;
const uint8_t pinosBotoes[NUM_BOTOES] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, A3, 1, 0, 16};

// Estado atual (debounced) e estado anterior de cada botão, para detecção de mudança
bool estadoAtualBotao[NUM_BOTOES];
bool estadoAnteriorBotao[NUM_BOTOES];

// Controle de debounce por millis()
unsigned long ultimaLeituraBotao[NUM_BOTOES];
const unsigned long DEBOUNCE_MS = 20; // tempo de estabilização do contato mecânico

// ============================================================================
// 3. MAPEAMENTO DE HARDWARE - ENTRADAS ANALÓGICAS (MANETES)
// ============================================================================
const uint8_t PINO_POTENCIA = A2; // Manete de Potência  -> Y Axis (fisicamente no pino A2)
const uint8_t PINO_MISTURA  = A1; // Manete de Mistura    -> X Axis
const uint8_t PINO_HELICE   = A0; // Manete de Passo de Hélice -> Z Axis (fisicamente no pino A0)

// ----------------------------------------------------------------------------
// >>> MODO DE CALIBRAÇÃO <<<
// Ative (true) para imprimir no Serial Monitor as leituras BRUTAS (0-1023)
// dos 3 potenciômetros em tempo real. Use para descobrir o mínimo e o
// máximo reais de cada manete antes de ajustar as constantes abaixo.
// Deixe em false para uso normal (modo HID).
// ----------------------------------------------------------------------------
#define MODO_CALIBRACAO false
const unsigned long INTERVALO_DEBUG_MS = 200; // intervalo entre impressões no Serial
unsigned long ultimoDebug = 0;

// ----------------------------------------------------------------------------
// >>> ÁREA DE CALIBRAÇÃO <<<
// Valores medidos no teste físico do curso mecânico real da caixa via
// Serial Monitor (modo calibração), já corrigidos pela inversão de pinos
// entre Potência e Hélice.
// ----------------------------------------------------------------------------
int potenciaMin = 389;   // Leitura bruta mínima do potenciômetro de Potência (Y)
int potenciaMax = 711;   // Leitura bruta máxima do potenciômetro de Potência (Y)

int misturaMin  = 319;   // Leitura bruta mínima do potenciômetro de Mistura (X)
int misturaMax  = 643;   // Leitura bruta máxima do potenciômetro de Mistura (X)

int heliceMin   = 256;   // Leitura bruta mínima do potenciômetro de Hélice (Z Axis)
int heliceMax   = 590;   // Leitura bruta máxima do potenciômetro de Hélice (Z Axis)
// ----------------------------------------------------------------------------

// Valor final (0-1023) já calibrado, enviado da última vez ao PC
int ultimoValorPotencia = -1;
int ultimoValorMistura  = -1;
int ultimoValorHelice   = -1;

// Tolerância de variação (deadband) para considerar que a manete realmente
// se moveu. Evita "tremida" no valor exibido no simulador.
const int DEADBAND = 10;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  #if MODO_CALIBRACAO
    Serial.begin(9600);
  #endif

  // Configura todos os pinos de botão como entrada com pull-up interno
  for (uint8_t i = 0; i < NUM_BOTOES; i++) {
    pinMode(pinosBotoes[i], INPUT_PULLUP);
    estadoAtualBotao[i]    = false; // false = solto (HIGH), true = pressionado (LOW)
    estadoAnteriorBotao[i] = false;
    ultimaLeituraBotao[i]  = 0;
  }

  // Define o range virtual dos eixos (0 a 1023)
  Joystick.setXAxisRange(0, 1023);
  Joystick.setYAxisRange(0, 1023);
  Joystick.setZAxisRange(0, 1023);

  Joystick.begin(false); // false = envio manual via sendState(), não automático
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
  #if MODO_CALIBRACAO
    // Modo calibração: só imprime as leituras brutas, não envia HID.
    if (millis() - ultimoDebug >= INTERVALO_DEBUG_MS) {
      ultimoDebug = millis();
      Serial.print("Potencia(A0): ");
      Serial.print(analogRead(PINO_POTENCIA));
      Serial.print("   Mistura(A1): ");
      Serial.print(analogRead(PINO_MISTURA));
      Serial.print("   Helice(A2): ");
      Serial.println(analogRead(PINO_HELICE));
    }
    return; // não processa botões nem envia estado HID em modo calibração
  #endif

  lerBotoes();

  // Cada eixo é lido e processado por uma função isolada, sem compartilhar
  // variáveis intermediárias entre si.
  processarPotencia();
  processarMistura();
  processarHelice();

  // Envia o relatório HID consolidado ao PC
  Joystick.sendState();
}

// ============================================================================
// FUNÇÃO: Varredura dos botões com State Change Detection + Debounce
// ============================================================================
void lerBotoes() {
  unsigned long agora = millis();

  for (uint8_t i = 0; i < NUM_BOTOES; i++) {
    // Leitura bruta: LOW = pressionado (contato fechado com GND)
    bool leituraBruta = (digitalRead(pinosBotoes[i]) == LOW);

    // Só reavalia o estado se já passou o tempo de debounce desde a última
    // mudança detectada nesse pino
    if (leituraBruta != estadoAtualBotao[i]) {
      if (agora - ultimaLeituraBotao[i] >= DEBOUNCE_MS) {
        estadoAtualBotao[i] = leituraBruta;
        ultimaLeituraBotao[i] = agora;
      }
    } else {
      // Reinicia a referência de tempo enquanto a leitura for estável
      ultimaLeituraBotao[i] = agora;
    }

    // Só envia o comando ao HID se o estado realmente mudou desde o último envio
    if (estadoAtualBotao[i] != estadoAnteriorBotao[i]) {
      Joystick.setButton(i, estadoAtualBotao[i] ? 1 : 0);
      estadoAnteriorBotao[i] = estadoAtualBotao[i];
    }
  }
}

// ============================================================================
// FUNÇÃO: Pino A0 -> lê, calibra (map+constrain), filtra e envia Y Axis (Potência)
// ============================================================================
void processarPotencia() {
  int bruto = analogRead(PINO_POTENCIA);
  int valor = map(bruto, potenciaMin, potenciaMax, 0, 1023);
  valor = constrain(valor, 0, 1023);

  if (ultimoValorPotencia == -1 || abs(valor - ultimoValorPotencia) > DEADBAND) {
    Joystick.setYAxis(valor);
    ultimoValorPotencia = valor;
  }
}

// ============================================================================
// FUNÇÃO: Pino A1 -> lê, calibra (map+constrain), filtra e envia X Axis (Mistura)
// ============================================================================
void processarMistura() {
  int bruto = analogRead(PINO_MISTURA);
  int valor = map(bruto, misturaMin, misturaMax, 0, 1023);
  valor = constrain(valor, 0, 1023);

  if (ultimoValorMistura == -1 || abs(valor - ultimoValorMistura) > DEADBAND) {
    Joystick.setXAxis(valor);
    ultimoValorMistura = valor;
  }
}

// ============================================================================
// FUNÇÃO: Pino A2 -> lê, calibra (map+constrain), filtra e envia Z Axis (Hélice)
// ============================================================================
void processarHelice() {
  int bruto = analogRead(PINO_HELICE);
  int valor = map(bruto, heliceMin, heliceMax, 0, 1023);
  valor = constrain(valor, 0, 1023);

  if (ultimoValorHelice == -1 || abs(valor - ultimoValorHelice) > DEADBAND) {
    Joystick.setZAxis(valor);
    ultimoValorHelice = valor;
  }
}
