/**
 * @file bateriaChokev3.c
 * @author Seu Nome (ou equipe)
 * @brief Código firmware para um módulo de bateria eletrônica com Arduino.
 * Implementa leitura de 11 pads (simples e dual-zone), tratamento de debounce,
 * lógica de choke para pratos, eliminação de crosstalk, detecção de rimshot
 * e controle de chimbal (aberto, fechado e pedal).
 * @version 3.0
 * @date 2025-07-21
 */

// Inclusão de bibliotecas (se houver, como <Arduino.h>)
// #include <Arduino.h>

// --- Grupos de Definições para Doxygen ---

/**
 * @defgroup PAD_INDICES Índices dos Pads
 * @brief Definições dos índices numéricos para cada pad da bateria.
 * @{
 */
#define BUMBO_PAD          0    /**< Índice para o pad do Bumbo. */
#define SURDO_PAD          1    /**< Índice para o pad do Surdo. */
#define TOM1_PAD           2    /**< Índice para o pad do Tom 1. */
#define TOM2_PAD           3    /**< Índice para o pad do Tom 2. */
#define CHIMBAL_PAD        4    /**< Índice para o pad do Chimbal. */
#define CAIXA_PAD          5    /**< Índice para o pad da Caixa (pele). */
#define ARO_CAIXA_PAD      6    /**< Índice para o sensor do Aro da Caixa. */
#define CONDUCAO_BORDA_PAD 7    /**< Índice para o sensor da Borda do prato de Condução. */
#define CONDUCAO_CUPULA_PAD 8   /**< Índice para o sensor da Cúpula do prato de Condução. */
#define ATAQUE_BORDA_PAD   9    /**< Índice para o sensor da Borda do prato de Ataque. */
#define ATAQUE_CUPULA_PAD  10   /**< Índice para o sensor da Cúpula do prato de Ataque. */
#define NUM_PADS          11    /**< Número total de sensores/pads no sistema. */
/** @} */

/**
 * @defgroup PIN_DEFINITIONS Definições de Pinos
 * @brief Mapeamento dos pinos do Arduino para os componentes da bateria.
 * @{
 */
const int PEDAL_CHIMBAL_PIN = 2; /**< Pino digital para o pedal do chimbal (com INPUT_PULLUP). */

/**
 * @brief Array que mapeia cada pad ao seu respectivo pino de entrada analógica.
 * A ordem dos pinos deve corresponder aos índices definidos em @ref PAD_INDICES.
 */
const int piezoPin[NUM_PADS] = {
  A0,  // BUMBO
  A1,  // SURDO
  A2,  // TOM1
  A3,  // TOM2
  A4,  // CHIMBAL
  A5,  // CAIXA
  A6,  // ARO_CAIXA
  A7,  // CONDUCAO_BORDA
  A8,  // CONDUCAO_CUPULA
  A9,  // ATAQUE_BORDA
  A10  // ATAQUE_CUPULA
};
/** @} */


/**
 * @defgroup MIDI_NOTES Notas MIDI
 * @brief Definições das notas MIDI para cada peça da bateria.
 * @{
 */
const int MIDI_NOTE_CHIMBAL_CLOSED = 42; /**< Nota MIDI para o chimbal fechado. */
const int MIDI_NOTE_CHIMBAL_OPEN   = 46; /**< Nota MIDI para o chimbal aberto. */
const int MIDI_NOTE_CHIMBAL_PEDAL  = 44; /**< Nota MIDI para o som do pedal do chimbal. */
const int MIDI_NOTE_RIMSHOT        = 40; /**< Nota MIDI para o som de rimshot da caixa. */

/**
 * @brief Array que define a nota MIDI padrão para cada pad.
 * A ordem deve corresponder aos índices definidos em @ref PAD_INDICES.
 */
const int midiNote[NUM_PADS] = {
  36,  // BUMBO
  41,  // SURDO
  43,  // TOM1
  45,  // TOM2
  MIDI_NOTE_CHIMBAL_CLOSED, // CHIMBAL (nota padrão, muda com o pedal)
  38,  // CAIXA
  39,  // ARO_CAIXA
  50,  // CONDUCAO_BORDA
  53,  // CONDUCAO_CUPULA
  49,  // ATAQUE_BORDA
  51   // ATAQUE_CUPULA
};
/** @} */

/**
 * @defgroup TUNING_PARAMETERS Parâmetros de Sensibilidade e Resposta
 * @brief Constantes para ajustar a sensibilidade, resposta e comportamento dos pads.
 * @{
 */

/**
 * @brief Limiar mínimo de leitura do sensor para registrar um toque.
 * Leituras abaixo deste valor são ignoradas. Ajustável por pad.
 */
const int threshold[NUM_PADS] = {
  120, 45, 230, 150, 80, 55, 40, 35, 35, 35, 35
};

/**
 * @brief Limiar de retrigger usado durante a checagem de repique.
 * Define o valor que uma nova leitura deve exceder para ser considerada um novo toque
 * logo após um toque inicial.
 * @note Os valores iniciais são baseados em `threshold[j] * 1.8`.
 */
const int retriggerThreshold[NUM_PADS] = {
  900, 950, 950, 950, 900, 550, 100, 950, 950, 680, 680
};

/** @brief Velocidade MIDI mínima a ser enviada. */
const int minVelocity = 10;
/** @brief Velocidade MIDI máxima a ser enviada. */
const int maxVelocity = 127;

/**
 * @brief Fator de ganho para amplificar o sinal de certos pads.
 * Útil para sensores com sinal naturalmente mais fraco, como os de cúpula.
 */
const float gainFactor[NUM_PADS] = {
  1, 1, 1, 1, 1, 1, 1, 1, 7, 1, 1.2
};

/**
 * @brief Janela de tempo (em ms) para detecção do valor de pico após um toque inicial.
 * A lógica é não-bloqueante.
 */
const unsigned long PEAK_DETECTION_WINDOW_MS = 7;

/** @brief Multiplicador mínimo para o cálculo do retrigger dinâmico, evitando toques duplos em intensidades baixas. */
const float RETRIGGER_MIN_MULTIPLIER = 1.5;
/** @} */


/**
 * @defgroup STATE_MACHINE Máquina de Estados e Debounce
 * @brief Definições e variáveis para a máquina de estados que controla cada pad.
 * @{
 */

/** @brief Enumerador para os estados de processamento de cada pad. */
enum PadState {
  PAD_STATE_IDLE,             /**< Ocioso, pronto para detectar um novo toque. */
  PAD_STATE_PEAK_DETECTION,   /**< Coletando leituras para encontrar o valor de pico do toque. */
  PAD_STATE_SILENT_DEBOUNCE,  /**< Período de silêncio para ignorar ruído imediato após um toque. */
  PAD_STATE_REPIQUE_CHECK,    /**< Período de verificação de repique com limiar dinâmico. */
  PAD_STATE_CHOKE_CONFIRMATION /**< Estado para confirmar se um toque foi um choke no prato. */
};

/** @brief Duração (em ms) do estado de silêncio total (debounce). */
const unsigned long SILENT_DEBOUNCE_MS = 30;
/** @brief Duração (em ms) do estado de checagem de repique. */
const unsigned long REPIQUE_CHECK_MS = 180;
/** @brief Duração (em ms) do tempo de confirmação do choke. */
const unsigned long CHOKE_CONFIRMATION_TIME_MS = 20;

/** @brief Array que armazena o estado atual de cada pad. @see PadState */
PadState padState[NUM_PADS];
/** @brief Array que armazena o tempo (millis()) da última mudança de estado de cada pad. */
unsigned long stateChangeTime[NUM_PADS];
/** @} */


/**
 * @defgroup CROSSTALK_LOGIC Lógica de Eliminação de Crosstalk
 * @brief Constantes e variáveis para o algoritmo de rejeição de crosstalk.
 * @{
 */
/** @brief Registra o tempo do último MIDI enviado com alta velocidade. */
unsigned long lastHighVelocityMidiTime = 0;
/** @brief Limiar de velocidade para considerar uma batida "forte". */
const int HIGH_VELOCITY_THRESHOLD = 115;
/** @brief Batidas com velocidade abaixo deste limiar serão descartadas se ocorrerem dentro da janela de crosstalk. */
const int LOW_VELOCITY_DISCARD_THRESHOLD = 29;
/** @brief Janela de tempo (em ms) após uma batida forte para ignorar batidas fracas (crosstalk). */
const unsigned long CROSSTALK_WINDOW_MS = 130;
/** @} */

/**
 * @defgroup GLOBAL_VARS Variáveis Globais de Estado
 * @brief Variáveis globais para controle de estado e lógica do programa.
 * @{
 */

/** @brief Array que armazena o valor de pico lido para cada pad. */
int peakSensorValues[NUM_PADS] = {0};
/** @brief Array que armazena o tempo (millis()) em que o pico foi detectado para cada pad. */
unsigned long peakFoundTime[NUM_PADS] = {0};
/** @brief Armazena o valor inicial do limiar de retrigger para o decaimento linear. */
int retriggerThresholdInitialDecay[NUM_PADS];

// --- Variáveis de Estado para o Pedal do Chimbal ---
int pedalChimbalState = HIGH;         /**< Estado atual do pedal do chimbal (HIGH = solto, LOW = pressionado). */
int lastPedalChimbalState = HIGH;     /**< Último estado registrado do pedal, para detecção de mudança. */
bool chimbalOpenSoundPlaying = false;   /**< Flag que indica se a nota de chimbal aberto está soando. */
bool chimbalClosedSoundPlaying = false; /**< Flag que indica se a nota de chimbal fechado está soando. */

// --- Flags para controle de notas (Choke) ---
bool conducaoBordaPlaying = false; /**< Flag que indica se a nota da borda da condução está soando. */
bool conducaoCupulaPlaying = false;/**< Flag que indica se a nota da cúpula da condução está soando. */
bool ataqueBordaPlaying = false;   /**< Flag que indica se a nota da borda do ataque está soando. */
bool ataqueCupulaPlaying = false;  /**< Flag que indica se a nota da cúpula do ataque está soando. */
bool caixaPlaying = false;         /**< Flag que indica se a nota da pele da caixa está soando. */
bool aroCaixaPlaying = false;      /**< Flag que indica se a nota do aro da caixa está soando. */
/** @} */


/**
 * @brief Função de inicialização do Arduino.
 * Configura a comunicação Serial/MIDI, os pinos dos sensores e inicializa
 * as variáveis de estado de todos os pads.
 */
void setup() {
  Serial.begin(31250); // Taxa de bauds padrão para comunicação MIDI via Serial

  for (int i = 0; i < NUM_PADS; i++) {
    pinMode(piezoPin[i], INPUT);
    padState[i] = PAD_STATE_IDLE;
    stateChangeTime[i] = 0;
    peakFoundTime[i] = 0;
    retriggerThresholdInitialDecay[i] = 0;
  }

  pinMode(PEDAL_CHIMBAL_PIN, INPUT_PULLUP);
}

/**
 * @brief Loop principal do programa.
 * Executa continuamente, lendo os sensores, processando a máquina de estados para
 * cada pad, tratando o pedal do chimbal e enviando as mensagens MIDI correspondentes.
 */
void loop() {
  unsigned long currentMillis = millis();

  // --- Processamento do Pedal do Chimbal (Digital) ---
  int currentPedalReading = digitalRead(PEDAL_CHIMBAL_PIN);
  if (currentPedalReading != lastPedalChimbalState) {
    lastPedalChimbalState = currentPedalReading;
    if (currentPedalReading == LOW) { // Pedal pressionado (Fechado)
      if (chimbalOpenSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0);
      }
      midiNoteOn(MIDI_NOTE_CHIMBAL_PEDAL, 30);
    } else { // Pedal Solto (Aberto)
      if (chimbalClosedSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0);
      }
    }
  }
  pedalChimbalState = currentPedalReading;

  // --- Processamento dos Pads Simples (Primeiro Loop) ---
  for (int j = 0; j <= CHIMBAL_PAD; j++) {
    int currentSensorReading = analogRead(piezoPin[j]);
    currentMillis = millis();

    switch (padState[j]) {
      case PAD_STATE_IDLE:
        if (currentSensorReading > threshold[j]) {
          peakSensorValues[j] = currentSensorReading;
          peakFoundTime[j] = currentMillis;
          padState[j] = PAD_STATE_PEAK_DETECTION;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_PEAK_DETECTION:
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorReading > peakSensorValues[j]) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
          }
        } else {
          if (peakSensorValues[j] > threshold[j]) {
            int sensorValueAdjusted = round(peakSensorValues[j] * gainFactor[j]);
            int velocity = map(sensorValueAdjusted, threshold[j], 1023, minVelocity, maxVelocity);
            velocity = constrain(velocity, minVelocity, maxVelocity);

            if (velocity < LOW_VELOCITY_DISCARD_THRESHOLD && (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
                padState[j] = PAD_STATE_IDLE;
            } else {
                if (j == CHIMBAL_PAD) {
                  if (pedalChimbalState == LOW) {
                    midiNoteOn(MIDI_NOTE_CHIMBAL_CLOSED, velocity);
                    if (chimbalOpenSoundPlaying) midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0);
                  } else {
                    midiNoteOn(MIDI_NOTE_CHIMBAL_OPEN, velocity);
                    if (chimbalClosedSoundPlaying) midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0);
                  }
                } else {
                  midiNoteOn(midiNote[j], velocity);
                }

                if (velocity > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                    lastHighVelocityMidiTime = currentMillis;
                }

                padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                stateChangeTime[j] = currentMillis;
                retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], peakSensorValues[j] * RETRIGGER_MIN_MULTIPLIER));
            }
          } else {
             padState[j] = PAD_STATE_IDLE;
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE;
        } else {
          long currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
          currentRetriggerThreshold = max((long)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), currentRetriggerThreshold);

          if (currentSensorReading > currentRetriggerThreshold) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
            padState[j] = PAD_STATE_PEAK_DETECTION;
            stateChangeTime[j] = currentMillis;
          }
        }
        break;
      
      // CHOKE_CONFIRMATION não se aplica a pads simples.
      case PAD_STATE_CHOKE_CONFIRMATION:
        padState[j] = PAD_STATE_IDLE;
        break;
    }
  }

  // --- Processamento de pads dual-zone (Segundo Loop) ---
  for (int j = CAIXA_PAD; j < NUM_PADS; j = j + 2) {
    int currentSensorPrincipalReading = analogRead(piezoPin[j]);
    int currentSensorSecundarioReading = analogRead(piezoPin[j+1]);
    currentMillis = millis();

    switch (padState[j]) {
      case PAD_STATE_IDLE:
        if (currentSensorPrincipalReading > threshold[j] || currentSensorSecundarioReading > threshold[j+1]) {
          peakSensorValues[j] = currentSensorPrincipalReading;
          peakSensorValues[j+1] = currentSensorSecundarioReading;
          peakFoundTime[j] = currentMillis;
          padState[j] = PAD_STATE_PEAK_DETECTION;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_PEAK_DETECTION:
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorPrincipalReading > peakSensorValues[j]) peakSensorValues[j] = currentSensorPrincipalReading;
          if (currentSensorSecundarioReading > peakSensorValues[j+1]) peakSensorValues[j+1] = currentSensorSecundarioReading;
        } else {
          if (peakSensorValues[j] <= threshold[j] && peakSensorValues[j+1] <= threshold[j+1]) {
            padState[j] = PAD_STATE_IDLE;
          } else {
            int velocityPrincipal = constrain(map(round(peakSensorValues[j] * gainFactor[j]), threshold[j], 1023, minVelocity, maxVelocity), minVelocity, maxVelocity);
            int velocitySecundario = constrain(map(round(peakSensorValues[j+1] * gainFactor[j+1]), threshold[j+1], 1023, minVelocity, maxVelocity), minVelocity, maxVelocity);

            if (max(velocitySecundario, velocityPrincipal) < LOW_VELOCITY_DISCARD_THRESHOLD && (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
                padState[j] = PAD_STATE_IDLE;
            } else {
                if (j == CAIXA_PAD) { // Lógica de Rimshot
                  if (peakSensorValues[CAIXA_PAD] > 600 && peakSensorValues[ARO_CAIXA_PAD] > 2 * threshold[ARO_CAIXA_PAD]) {
                    midiNoteOn(MIDI_NOTE_RIMSHOT, max(velocityPrincipal, velocitySecundario));
                  } else if (peakSensorValues[j] < 1000 && peakSensorValues[j+1] * 1.1 > peakSensorValues[j]) {
                    midiNoteOn(midiNote[j+1], velocitySecundario); // Aro
                  } else {
                    midiNoteOn(midiNote[j], velocityPrincipal); // Caixa
                  }
                } else { // Lógica para Condução e Ataque (com Choke)
                    if (peakSensorValues[j] < 1000 && peakSensorValues[j+1] > peakSensorValues[j]) {
                      midiNoteOn(midiNote[j+1], velocitySecundario); // Cúpula
                    } else if (peakSensorValues[j+1] < peakSensorValues[j] * 0.05) { // Potencial choke
                      // A linha abaixo está comentada no código original, mantive assim.
                      // padState[j] = PAD_STATE_CHOKE_CONFIRMATION;
                      midiNoteOff(midiNote[j], 0);
                      midiNoteOff(midiNote[j+1], 0);
                      stateChangeTime[j] = currentMillis;
                    } else {
                      midiNoteOn(midiNote[j], velocityPrincipal); // Borda
                    }
                }

                if (max(velocitySecundario, velocityPrincipal) > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                    lastHighVelocityMidiTime = currentMillis;
                }

                if (padState[j] != PAD_STATE_CHOKE_CONFIRMATION) {
                    padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                    stateChangeTime[j] = currentMillis;
                }

                retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], max(peakSensorValues[j], peakSensorValues[j+1]) * RETRIGGER_MIN_MULTIPLIER));
                retriggerThresholdInitialDecay[j+1] = retriggerThresholdInitialDecay[j];
            }
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE;
        } else {
          long currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
          currentRetriggerThreshold = max((long)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), currentRetriggerThreshold);

          if (max(currentSensorPrincipalReading, currentSensorSecundarioReading) > currentRetriggerThreshold) {
            peakSensorValues[j] = currentSensorPrincipalReading;
            peakSensorValues[j+1] = currentSensorSecundarioReading;
            peakFoundTime[j] = currentMillis;
            padState[j] = PAD_STATE_PEAK_DETECTION;
            stateChangeTime[j] = currentMillis;
          }
        }
        break;

       case PAD_STATE_CHOKE_CONFIRMATION:
        if (j == CONDUCAO_BORDA_PAD || j == ATAQUE_BORDA_PAD) {
          if (analogRead(piezoPin[j]) > peakSensorValues[j]) peakSensorValues[j] = analogRead(piezoPin[j]);
          if (analogRead(piezoPin[j+1]) > peakSensorValues[j+1]) peakSensorValues[j+1] = analogRead(piezoPin[j+1]);

          if (currentMillis - stateChangeTime[j] >= CHOKE_CONFIRMATION_TIME_MS) {
            if (peakSensorValues[j+1] < (peakSensorValues[j] * 0.05) || peakSensorValues[j+1] < 20) {
              if (j == CONDUCAO_BORDA_PAD) {
                  midiNoteOff(midiNote[CONDUCAO_BORDA_PAD], 0);
                  midiNoteOff(midiNote[CONDUCAO_CUPULA_PAD], 0);
              } else if (j == ATAQUE_BORDA_PAD) {
                  midiNoteOff(midiNote[ATAQUE_BORDA_PAD], 0);
                  midiNoteOff(midiNote[ATAQUE_CUPULA_PAD], 0);
              }
              padState[j] = PAD_STATE_IDLE;
            } else {
              if (peakSensorValues[j] > threshold[j] || peakSensorValues[j+1] > threshold[j+1]) {
                 padState[j] = PAD_STATE_PEAK_DETECTION;
                 stateChangeTime[j] = currentMillis;
              } else {
                 padState[j] = PAD_STATE_IDLE;
              }
            }
            peakSensorValues[j] = 0;
            peakSensorValues[j+1] = 0;
          }
        } else {
          padState[j] = PAD_STATE_IDLE;
          peakSensorValues[j] = 0;
          peakSensorValues[j+1] = 0;
        }
        break;
    }
  }
}

/**
 * @brief Envia uma mensagem MIDI Note On pela porta Serial.
 * @param note O número da nota MIDI (0-127).
 * @param velocity A velocidade da nota (0-127).
 * @note Também atualiza as flags globais de estado `...Playing` para o controle de choke.
 */
void midiNoteOn(int note, int velocity) {
  byte channel = 0;
  Serial.write(0x90 | channel);
  Serial.write((byte)note);
  Serial.write((byte)velocity);

  // Atualiza flags de notas tocando
  if (note == midiNote[CONDUCAO_BORDA_PAD]) conducaoBordaPlaying = true;
  else if (note == midiNote[CONDUCAO_CUPULA_PAD]) conducaoCupulaPlaying = true;
  else if (note == midiNote[ATAQUE_BORDA_PAD]) ataqueBordaPlaying = true;
  else if (note == midiNote[ATAQUE_CUPULA_PAD]) ataqueCupulaPlaying = true;
  else if (note == midiNote[CAIXA_PAD]) caixaPlaying = true;
  else if (note == midiNote[ARO_CAIXA_PAD]) aroCaixaPlaying = true;
  else if (note == MIDI_NOTE_CHIMBAL_CLOSED) chimbalClosedSoundPlaying = true;
  else if (note == MIDI_NOTE_CHIMBAL_OPEN) chimbalOpenSoundPlaying = true;
}

/**
 * @brief Envia uma mensagem MIDI Note Off pela porta Serial.
 * @param note O número da nota MIDI (0-127) a ser desligada.
 * @param velocity A velocidade de "release" da nota (geralmente 0).
 * @note Também atualiza as flags globais de estado `...Playing` para o controle de choke.
 */
void midiNoteOff(int note, int velocity) {
  byte channel = 0;
  Serial.write(0x80 | channel);
  Serial.write((byte)note);
  Serial.write((byte)velocity);

  // Atualiza flags de notas parando de tocar
  if (note == midiNote[CONDUCAO_BORDA_PAD]) conducaoBordaPlaying = false;
  else if (note == midiNote[CONDUCAO_CUPULA_PAD]) conducaoCupulaPlaying = false;
  else if (note == midiNote[ATAQUE_BORDA_PAD]) ataqueBordaPlaying = false;
  else if (note == midiNote[ATAQUE_CUPULA_PAD]) ataqueCupulaPlaying = false;
  else if (note == midiNote[CAIXA_PAD]) caixaPlaying = false;
  else if (note == midiNote[ARO_CAIXA_PAD]) aroCaixaPlaying = false;
  else if (note == MIDI_NOTE_CHIMBAL_CLOSED) chimbalClosedSoundPlaying = false;
  else if (note == MIDI_NOTE_CHIMBAL_OPEN) chimbalOpenSoundPlaying = false;
}
