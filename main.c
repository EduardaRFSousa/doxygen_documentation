/**
 * @file bateriaChokev3.c
 * @author Seu Nome (ou equipe)
 * @brief Código firmware para um módulo de bateria eletrônica com Arduino.
 * Implementa leitura de 11 pads (simples e dual-zone), tratamento de debounce,
 * lógica de choke para pratos, eliminação de crosstalk, detecção de rimshot
 * e controle de chimbal (aberto, fechado e pedal).
 * @version 3.0
 * @date 2025-07-23
 */

// Inclusão de bibliotecas (se houver, como <Arduino.h>)
// #include <Arduino.h>

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
  /** @brief O Chimbal possui lógica especial para sons aberto/fechado com base no pedal. */
  // --- Processamento do Pedal do Chimbal (Digital) ---
  int currentPedalReading = digitalRead(PEDAL_CHIMBAL_PIN); // Lê o estado do pedal do chimbal
  if (currentPedalReading != lastPedalChimbalState) {
    lastPedalChimbalState = currentPedalReading;
    if (currentPedalReading == LOW) { // Pedal pressionado (Fechado)
      if (chimbalOpenSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0); // Envia nota MIDI para pedal pressionado
      }
      midiNoteOn(MIDI_NOTE_CHIMBAL_PEDAL, 30);
    } else { // Pedal Solto (Aberto)
      if (chimbalClosedSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0); // Envia nota MIDI para pedal solto
      }
    }
  }
  pedalChimbalState = currentPedalReading; // Atualiza o último estado do pedal

  /**
   * @defgroup LOOP_PADS_SIMPLES Processamento dos Pads Simples
   * @brief Loop para a detecção e processamento de pads com uma única zona.
   * @details Cada pad simples passa por uma máquina de estados para garantir detecção precisa: @see PadState
   * @{
   */
  for (int j = 0; j <= CHIMBAL_PAD; j++) {
    int currentSensorReading = analogRead(piezoPin[j]); // Sempre lê o sensor
    currentMillis = millis(); // Atualiza o tempo

    switch (padState[j]) {
      case PAD_STATE_IDLE:
      /** 
       * @brief Estado Ocioso: Aguarda o início de um toque.
       * @details Se a leitura do sensor ultrapassar o `threshold`, o pad
       * transiciona para `PAD_STATE_PEAK_DETECTION` para capturar a intensidade máxima da batida.
       */
        if (currentSensorReading > threshold[j]) {
          // Detectou um toque inicial, inicia a detecção de pico
          peakSensorValues[j] = currentSensorReading;
          peakFoundTime[j] = currentMillis;
          padState[j] = PAD_STATE_PEAK_DETECTION;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_PEAK_DETECTION:
      /**
       * @brief Estado de Detecção de Pico: Encontra a intensidade máxima do toque.
       * @details O pad permanece neste estado por `PEAK_DETECTION_WINDOW_MS` (7ms),
       * registrando o valor mais alto lido do sensor.
       * - Se o pico validado for menor ou igual ao `threshold`, o pad retorna a `IDLE` (falso positivo).
       * - Caso contrário, calcula a velocidade MIDI, aplica a lógica de eliminação de crosstalk
       * e envia a nota MIDI correspondente. Transiciona para `PAD_STATE_SILENT_DEBOUNCE`.
       */
      // Continua buscando o pico dentro da janela
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorReading > peakSensorValues[j]) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
          }
        } else {
          if (peakSensorValues[j] > threshold[j]) {
            // Sinal validado pela detecção de pico.
            int sensorValueAdjusted = round(peakSensorValues[j] * gainFactor[j]);
            int velocity = map(sensorValueAdjusted, threshold[j], 1023, minVelocity, maxVelocity);
            velocity = constrain(velocity, minVelocity, maxVelocity);

            /**
             * @brief Lógica de Eliminação de Crosstalk para Pads Simples.
             * @details Ignora batidas de baixa velocidade que ocorrem dentro de `CROSSTALK_WINDOW_MS` 
             * após uma batida forte em outro pad, prevenindo disparos indesejados por vibração.
             */
            if (velocity < LOW_VELOCITY_DISCARD_THRESHOLD && (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
              // Esta leitura é uma batida fraca dentro da janela de crosstalk, ignora
              padState[j] = PAD_STATE_IDLE;
            } else {
              /**
               * @brief Envio de Nota MIDI para Pads Simples.
               * @details Se não for crosstalk, a nota MIDI é disparada.
               * Possui lógica especial para o Chimbal (aberto/fechado) baseada no pedal.
               */
                if (j == CHIMBAL_PAD) {
                  if (pedalChimbalState == LOW) { // Pedal Fechado: Toca chimbal fechado
                    midiNoteOn(MIDI_NOTE_CHIMBAL_CLOSED, velocity);
                    if (chimbalOpenSoundPlaying) midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0); // Garante que a nota de chimbal aberto seja desligada
                  } else { // Pedal Solto: Toca chimbal aberto
                    midiNoteOn(MIDI_NOTE_CHIMBAL_OPEN, velocity);
                    if (chimbalClosedSoundPlaying) midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0); // Garante que a nota de chimbal fechado seja desligada
                  }
                } else {
                  midiNoteOn(midiNote[j], velocity);
                }

              /**
               * @brief Atualização do tempo da última batida forte.
               * @details Usado na lógica de crosstalk para determinar a janela de descarte.
               */
                if (velocity > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                    lastHighVelocityMidiTime = currentMillis;
                }

                // Transiciona para o debounce silencioso após disparar a nota
                padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                stateChangeTime[j] = currentMillis;

                // Armazena o valor inicial do retrigger para o decaimento linear
                retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], peakSensorValues[j] * RETRIGGER_MIN_MULTIPLIER));
            }
          } else {
            // Janela de detecção de pico encerrou.
            padState[j] = PAD_STATE_IDLE;
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
      /**
       * @brief Estado de Debounce Silencioso: Ignora ruídos residuais.
       * @details O pad permanece neste estado por `SILENT_DEBOUNCE_MS` (20ms),
       * prevenindo múltiplos disparos de uma única batida. Após o tempo, transiciona
       * para `PAD_STATE_REPIQUE_CHECK`.
       */
      // Verifica se o tempo do debounce silencioso já passou
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          // Transiciona para o estado de checagem de repique
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
      /**
       * @brief Estado de Verificação de Repique: Permite batidas rápidas.
       * @details Durante `REPIQUE_CHECK_MS` (60ms), o limiar de detecção
       * (`currentRetriggerThreshold`) decai de um valor mais alto
       * (`retriggerThresholdInitialDecay`) para o `threshold` normal.
       * Isso permite que batidas rápidas (repique) sejam detectadas com precisão,
       * enquanto ruídos residuais menores são ignorados.
       * - Se uma nova leitura ultrapassar o `currentRetriggerThreshold`, transiciona para `PAD_STATE_PEAK_DETECTION`.
       * - Se o tempo de `REPIQUE_CHECK_MS` expirar sem um novo toque válido, o pad retorna a `PAD_STATE_IDLE`.
       */
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE; // Retorna ao estado ocioso se o tempo de repique acabou
        } else {
          long currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
          currentRetriggerThreshold = max((long)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), currentRetriggerThreshold);

          // Se houver repique, o estado muda para peak_detection
          if (currentSensorReading > currentRetriggerThreshold) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
            padState[j] = PAD_STATE_PEAK_DETECTION;
            stateChangeTime[j] = currentMillis;
          }
        }
        break;

      // O estado CHOKE_CONFIRMATION não é aplicável a pads simples, então não há case aqui.
      case PAD_STATE_CHOKE_CONFIRMATION:
        padState[j] = PAD_STATE_IDLE;
        break;
    }
  }
  /** @} */ // Fim do grupo LOOP_PADS_SIMPLES

  /**
   * @defgroup LOOP_PADS_DUAL_ZONE Processamento dos Pads Dual-Zone
   * @brief Loop para a detecção e processamento de pads com duas zonas (Caixa, Condução, Ataque).
   * @details Estes pads são processados em pares (principal e secundário) e
   * possuem lógicas complexas para diferenciar entre as zonas de toque (ex: pele/aro, borda/cúpula)
   * e funcionalidades avançadas como Rimshot e Choke.
   * A máquina de estados é similar aos pads simples, mas adaptada para dois sensores,
   * e inclui o estado `PAD_STATE_CHOKE_CONFIRMATION` para pratos.
   * @{
   */
  // --- Processamento de pads dual-zone (Segundo Loop) ---
  for (int j = CAIXA_PAD; j < NUM_PADS; j = j + 2) {
    int currentSensorPrincipalReading = analogRead(piezoPin[j]);
    int currentSensorSecundarioReading = analogRead(piezoPin[j+1]);
    currentMillis = millis(); // Atualiza o tempo

    switch (padState[j]) {
      case PAD_STATE_IDLE:
      /**
       * @brief Estado Ocioso: Aguarda o início de um toque em qualquer zona.
       * @details Se a leitura de qualquer um dos sensores (principal ou secundário)
       * ultrapassar seu respectivo `threshold`, o pad transiciona para
       * `PAD_STATE_PEAK_DETECTION`.
       */
        if (currentSensorPrincipalReading > threshold[j] || currentSensorSecundarioReading > threshold[j+1]) {
          // Detectou um toque inicial, inicia a detecção de pico
          peakSensorValues[j] = currentSensorPrincipalReading;
          peakSensorValues[j+1] = currentSensorSecundarioReading;
          peakFoundTime[j] = currentMillis;
          padState[j] = PAD_STATE_PEAK_DETECTION;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_PEAK_DETECTION:
      /**
       * @brief Estado de Detecção de Pico: Encontra as intensidades máximas de ambas as zonas.
       * @details O pad permanece neste estado por `PEAK_DETECTION_WINDOW_MS` (7ms),
       * registrando os valores mais altos de ambos os sensores.
       * - Se ambos os picos validados estiverem abaixo do `threshold`, retorna a `IDLE`.
       * - Caso contrário, calcula as velocidades MIDI para ambas as zonas, aplica a lógica
       * de crosstalk e determina qual som MIDI enviar (pele, aro, cúpula, rimshot, potencial choke).
       * Transiciona para `PAD_STATE_SILENT_DEBOUNCE` ou `PAD_STATE_CHOKE_CONFIRMATION` (se ativado).
       */
        // Continua buscando o pico dentro da janela
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorPrincipalReading > peakSensorValues[j]) peakSensorValues[j] = currentSensorPrincipalReading;
          if (currentSensorSecundarioReading > peakSensorValues[j+1]) peakSensorValues[j+1] = currentSensorSecundarioReading;
        } else {
          // Janela de detecção de pico encerrou.
          if (peakSensorValues[j] <= threshold[j] && peakSensorValues[j+1] <= threshold[j+1]) {
            padState[j] = PAD_STATE_IDLE;
          } else {
            // Sinal validado pela detecção de pico.
            int velocityPrincipal = constrain(map(round(peakSensorValues[j] * gainFactor[j]), threshold[j], 1023, minVelocity, maxVelocity), minVelocity, maxVelocity);
            int velocitySecundario = constrain(map(round(peakSensorValues[j+1] * gainFactor[j+1]), threshold[j+1], 1023, minVelocity, maxVelocity), minVelocity, maxVelocity);

            /**
             * @brief Lógica de Eliminação de Crosstalk para Pads Dual-Zone.
             * @details Ignora batidas de baixa velocidade (considerando a maior velocidade
             * entre as duas zonas) que ocorrem dentro da janela de crosstalk.
             */
            if (max(velocitySecundario, velocityPrincipal) < LOW_VELOCITY_DISCARD_THRESHOLD && (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
                padState[j] = PAD_STATE_IDLE;
            } else {
              /**
               * @brief Envio de Nota MIDI e Lógica Específica para Pads Dual-Zone.
               * @details Determina o som a ser emitido (pele, aro, cúpula, rimshot)
               * com base na comparação das intensidades dos dois sensores.
               */
              // *** LÓGICA DE RIMSHOT PARA CAIXA_PAD ***
              if (j == CAIXA_PAD) {
                /**
                 * @brief Lógica de Rimshot para a Caixa.
                 * @details Ativa o som de Rimshot se ambos os sensores (pele e aro)
                 * detectarem um golpe forte e simultâneo.
                 * Caso contrário, diferencia entre som de aro (se o aro for dominante)
                 * ou som da pele (borda normal).
                 * @note Os valores de 600 são sugestões para "golpe forte" e podem ser ajustados.
                 */
                if (peakSensorValues[CAIXA_PAD] > 600 && peakSensorValues[ARO_CAIXA_PAD] > 2 * threshold[ARO_CAIXA_PAD]) {
                  midiNoteOn(MIDI_NOTE_RIMSHOT, max(velocityPrincipal, velocitySecundario));
                } 
                // Lógica para som de cúpula/aro (se o aro for dominante)
                else if (peakSensorValues[j] < 1000 && peakSensorValues[j+1] * 1.1 > peakSensorValues[j]) {
                  midiNoteOn(midiNote[j+1], velocitySecundario); // Toca o aro
                } 
                // Lógica para borda normal (golpe principal na pele)
                else {
                  midiNoteOn(midiNote[j], velocityPrincipal); // Toca a caixa
                }
              } // *** LÓGICA PARA CONDUÇÃO E ATAQUE (COM CHOKE CONFIRMATION) ***
              else { // Para CONDUCAO_BORDA_PAD e ATAQUE_BORDA_PAD
                /**
                 * @brief Lógica de Sons e Potencial Choke para Pratos (Condução e Ataque).
                 * @details Diferencia entre som de cúpula (se a cúpula for dominante)
                 * e som de borda.
                 * Se o sinal da cúpula for muito baixo em relação à borda, indica um potencial choke
                 * e as notas são desligadas.
                 * @warning A transição para `PAD_STATE_CHOKE_CONFIRMATION` está desativada no código e requer revisão.
                 */
                  if (peakSensorValues[j] < 1000 && peakSensorValues[j+1] > peakSensorValues[j]) { // som de cupula
                    midiNoteOn(midiNote[j+1], velocitySecundario);
                  } // Potencial choke (se o secundário for muito fraco em relação ao primário)
                  else if (peakSensorValues[j+1] < peakSensorValues[j] * 0.05) { // Potencial choke
                    //padState[j] = PAD_STATE_CHOKE_CONFIRMATION;
                    /** @todo Corrigir transição para PAD_STATE_CHOKE_CONFIRMATION */
                    // Mesmo com o bug na transição, tenta desligar as notas imediatamente
                    midiNoteOff(midiNote[j], 0);
                    midiNoteOff(midiNote[j+1], 0);
                    stateChangeTime[j] = currentMillis; // Marca o início do período de (falha de) confirmação
                  } else { // borda maior (não é choke e não é cúpula)
                    midiNoteOn(midiNote[j], velocityPrincipal);
                  }
              }

              /**
               * @brief Atualização do tempo da última batida forte para Dual-Zone.
               * @details Usado na lógica de crosstalk. Considera a maior velocidade entre as zonas.
               */
              if (max(velocitySecundario, velocityPrincipal) > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                  lastHighVelocityMidiTime = currentMillis;
              }

              // Transiciona para o debounce silencioso após disparar a nota
              // APENAS SE NÃO ENTROU NO CHOKE_CONFIRMATION (para Condução/Ataque, se ativado)
              if (padState[j] != PAD_STATE_CHOKE_CONFIRMATION) { // Garante que não sobrescreve o estado se já estiver em CHOKE_CONFIRMATION
                  padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                  stateChangeTime[j] = currentMillis;
              }

              // Armazena o valor inicial do retrigger para o decaimento linear
              retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], max(peakSensorValues[j], peakSensorValues[j+1]) * RETRIGGER_MIN_MULTIPLIER));
              retriggerThresholdInitialDecay[j+1] = retriggerThresholdInitialDecay[j]; // Aplica ao secundário também para consistência
            }
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
        /**
         * @brief Estado de Debounce Silencioso: Ignora ruídos residuais para Dual-Zone.
         * @details Comportamento similar ao pad simples, garantindo um período de
         * silêncio após um toque validado antes de checar por repiques.
         * Transiciona para `PAD_STATE_REPIQUE_CHECK`.
         */
        // Verifica se o tempo do debounce silencioso já passou
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          // Transiciona para o estado de checagem de repique
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
        /**
         * @brief Estado de Verificação de Repique: Permite batidas rápidas para Dual-Zone.
         * @details O `currentRetriggerThreshold` decai linearmente, similar aos pads simples.
         * No entanto, a detecção de um novo repique considera o maior pico entre os dois sensores.
         * - Se `max(currentSensorPrincipalReading, currentSensorSecundarioReading)`
         * ultrapassar o limiar, transiciona para `PAD_STATE_PEAK_DETECTION`.
         * - Retorna a `IDLE` se o tempo de repique expirar.
         */
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE;
        } else {
          long currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
          currentRetriggerThreshold = max((long)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), currentRetriggerThreshold);

          // Se houver repique, o estado muda para peak_detection
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
        /**
         * @brief Estado de Confirmação de Choke (Exclusivo para Pratos de Condução e Ataque).
         * @details Este estado, embora atualmente desativado na transição de `PEAK_DETECTION` (bug),
         * é projetado para verificar, após um breve período (`CHOKE_CONFIRMATION_TIME_MS`),
         * se um "choke" (abafamento do prato) realmente ocorreu.
         * - Continua monitorando os picos dos dois sensores.
         * - Após o tempo, se o pico do sensor secundário (cúpula) for muito baixo em relação
         * ao principal (borda) ou em valor absoluto, o choke é confirmado e as notas MIDI são desligadas.
         * - Caso contrário, o evento é reavaliado como um possível toque normal ou o pad retorna a `IDLE`.
         */
        // Certifica-se de que este estado só seja processado para pads de Condução e Ataque
        if (j == CONDUCAO_BORDA_PAD || j == ATAQUE_BORDA_PAD) {
          // Continuar buscando o pico dos dois sensores durante o tempo de confirmação
          if (analogRead(piezoPin[j]) > peakSensorValues[j]) peakSensorValues[j] = analogRead(piezoPin[j]);
          if (analogRead(piezoPin[j+1]) > peakSensorValues[j+1]) peakSensorValues[j+1] = analogRead(piezoPin[j+1]);

          // Se o tempo de confirmação passou
          if (currentMillis - stateChangeTime[j] >= CHOKE_CONFIRMATION_TIME_MS) {
            // Reavalia o estado do sensor secundário usando os picos finais para confirmar o choke
            // NOTA: peakSensorValues[j] aqui é o pico máximo atingido no primário durante a janela de choke
            //       peakSensorValues[j+1] aqui é o pico máximo atingido no secundário durante a janela de choke
            if (peakSensorValues[j+1] < (peakSensorValues[j] * 0.05) || peakSensorValues[j+1] < 20) {
              // Choke confirmado: Enviar MIDI Note Offs para as notas que estão tocando neste pad
              if (j == CONDUCAO_BORDA_PAD) {
                  midiNoteOff(midiNote[CONDUCAO_BORDA_PAD], 0);
                  midiNoteOff(midiNote[CONDUCAO_CUPULA_PAD], 0);
              } else if (j == ATAQUE_BORDA_PAD) {
                  midiNoteOff(midiNote[ATAQUE_BORDA_PAD], 0);
                  midiNoteOff(midiNote[ATAQUE_CUPULA_PAD], 0);
              }
              padState[j] = PAD_STATE_IDLE; // Volta ao IDLE após o choke
            } else {
              // Não foi choke ou o secundário se tornou relevante (não houve "choke" claro)
              // Se houver nova atividade significativa, transiciona para PEAK_DETECTION, senão IDLE.
              if (peakSensorValues[j] > threshold[j] || peakSensorValues[j+1] > threshold[j+1]) {
                 padState[j] = PAD_STATE_PEAK_DETECTION; // Volta para detectar o pico real do que aconteceu
                 stateChangeTime[j] = currentMillis; // Reinicia o tempo para PEAK_DETECTION
              } else {
                 padState[j] = PAD_STATE_IDLE; // Volta para IDLE se não houver mais atividade significativa
              }
            }
            // Resetar peakSensorValues para este pad (e seu secundário) para que o próximo ciclo comece do zero
            peakSensorValues[j] = 0;
            peakSensorValues[j+1] = 0;
          }
          // Se o tempo ainda não passou, permanece neste estado lendo os sensores e atualizando os picos.
        } else { // Se algum pad que não deveria estar aqui (ex: Caixa) entra neste estado, volta para IDLE
          padState[j] = PAD_STATE_IDLE;
          peakSensorValues[j] = 0; // Resetar picos para pad inválido no choke state
          peakSensorValues[j+1] = 0;
        }
        break;
    }
  }
  /** @} */ // Fim do grupo LOOP_PADS_DUAL_ZONE
} // Fim do void loop()

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