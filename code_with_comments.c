// --- Definição dos Índices para cada Pad ---
#define BUMBO_PAD          0
#define SURDO_PAD          1
#define TOM1_PAD           2
#define TOM2_PAD           3
#define CHIMBAL_PAD        4
#define CAIXA_PAD          5
#define ARO_CAIXA_PAD      6
#define CONDUCAO_BORDA_PAD 7
#define CONDUCAO_CUPULA_PAD 8
#define ATAQUE_BORDA_PAD   9
#define ATAQUE_CUPULA_PAD  10
#define NUM_PADS          11 // Atualizado para 11 pads no total

// --- Definição do Pino Digital para o Pedal do Chimbal ---
const int PEDAL_CHIMBAL_PIN = 2;

// --- Notas MIDI Específicas para o Chimbal ---
const int MIDI_NOTE_CHIMBAL_CLOSED = 42;
const int MIDI_NOTE_CHIMBAL_OPEN   = 46;
const int MIDI_NOTE_CHIMBAL_PEDAL  = 44;
const int MIDI_NOTE_RIMSHOT        = 40; // NOVO: Nota MIDI para o Rimshot

// --- Definição das Portas Analógicas dos Piezo (Array) ---
const int piezoPin[NUM_PADS] = {
  A0,  // BUMBO
  A1,  // SURDO
  A2,  // TOM1
  A3,  // TOM2
  A4,  // CHIMBAL
  A5,  // CAIXA
  A6,  // ARO_CAIXA
  A7,  // CONDUCAO_BORDA
  A8,  // CONDUCAO_CUPULA (Novo pino)
  A9,  // ATAQUE_BORDA
  A10  // ATAQUE_CUPULA (Novo pino)
};

// --- Definição dos Limiares de Sensibilidade (Array) ---
const int threshold[NUM_PADS] = {
  120, // BUMBO
  45,  // SURDO
  230, // TOM1
  150, // TOM2
  80,  // CHIMBAL
  55,  // CAIXA
  40,  // ARO_CAIXA
  35,  // CONDUCAO_BORDA (Mesmo valor original para Condução)
  35,  // CONDUCAO_CUPULA (Novo valor, assumindo similar à borda)
  35,  // ATAQUE_BORDA (Mesmo valor original para Ataque)
  35   // ATAQUE_CUPULA (Novo valor, assumindo similar à borda)
};

// --- Limiares de Retrigger para Nova Batida (Array) ---
// Valor que uma nova leitura deve exceder (durante REPIQUE_CHECK_MS) para ser considerada um novo disparo
// Estes valores podem ser ajustados individualmente para cada pad,
// para melhor sensibilidade a golpes fortes durante o período de repique.
// Os valores iniciais são baseados em (threshold[j] * 1.8) como no RETRIGGER_THRESHOLD_FACTOR anterior.
const int retriggerThreshold[NUM_PADS] = {
  900, // BUMBO (aprox. 120 * 1.8)
  950, // SURDO (aprox. 150 * 1.8)
  950, // TOM1 (aprox. 250 * 1.8)
  950, // TOM2 (aprox. 250 * 1.8)
  900, // CHIMBAL (aprox. 90 * 1.8)
  550, // CAIXA (aprox. 95 * 1.8)
  100, // ARO_CAIXA (aprox. 50 * 1.8)
  950, // CONDUCAO_BORDA (Mesmo valor original para Condução)
  950, // CONDUCAO_CUPULA (Novo valor, assumindo similar à borda)
  680, // ATAQUE_BORDA (Mesmo valor original para Ataque)
  680  // ATAQUE_CUPULA (Novo valor, assumindo similar à borda)
};

// --- Definição dos Números das Notas MIDI (Array) ---
const int midiNote[NUM_PADS] = {
  36,  // BUMBO
  41,  // SURDO
  43,  // TOM1
  45,  // TOM2
  MIDI_NOTE_CHIMBAL_CLOSED, // CHIMBAL
  38,  // CAIXA
  39,  // ARO_CAIXA
  50,  // CONDUCAO_BORDA (Nota MIDI original da Condução)
  53,  // CONDUCAO_CUPULA (Nova nota MIDI para a cúpula da condução)
  49,  // ATAQUE_BORDA (Nota MIDI original do Ataque)
  51   // ATAQUE_CUPULA (Nova nota MIDI para a cúpula do ataque)
};

// --- Definição da Velocidade Mínima e Máxima MIDI ---
const int minVelocity = 10;
const int maxVelocity = 127;

// --- ARRAY PARA FATOR DE GANHO ---
const float gainFactor[NUM_PADS] = {
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  7, // CONDUCAO_CUPULA
  1,
  1.2  // ATAQUE_CUPULA
};

// --- Constante para a janela de detecção de pico (agora não-bloqueante) ---
const unsigned long PEAK_DETECTION_WINDOW_MS = 7;

// Arrays para armazenar os valores de pico
int peakSensorValues[NUM_PADS] = {0};

// Armazena o tempo em que o pico foi encontrado
unsigned long peakFoundTime[NUM_PADS] = {0};

// --- ENUMERADOR PARA OS ESTADOS DO DEBOUNCE ---
enum PadState {
  PAD_STATE_IDLE,             // Pad pronto para detectar um novo toque
  PAD_STATE_PEAK_DETECTION,   // Coletando leituras para encontrar o valor de pico
  PAD_STATE_SILENT_DEBOUNCE,  // Período inicial de silêncio total
  PAD_STATE_REPIQUE_CHECK,    // Período de verificação de repique com limiar mais alto
  PAD_STATE_CHOKE_CONFIRMATION // NOVO: Estado para confirmar o choke
};

// --- Variáveis de estado para cada pad ---
PadState padState[NUM_PADS];
unsigned long stateChangeTime[NUM_PADS];
// Tempo em que o estado do pad mudou pela última vez

// --- DURAÇÕES DOS ESTADOS DE DEBOUNCE ---
const unsigned long SILENT_DEBOUNCE_MS = 30; // Duração do estado de silêncio total (ALTERADO PARA 20MS)
const unsigned long REPIQUE_CHECK_MS = 180; // Duração do estado de checagem de repique
const unsigned long CHOKE_CONFIRMATION_TIME_MS = 20; // NOVO: Tempo para confirmar o choke

// REMOVIDO: const float RETRIGGER_THRESHOLD_FACTOR = 4;

// --- NOVAS CONSTANTES PARA A LÓGICA DE ELIMINAÇÃO DE CROSSTALK ---
unsigned long lastHighVelocityMidiTime = 0; // Armazena o tempo do último MIDI enviado com alta velocidade
const int HIGH_VELOCITY_THRESHOLD = 115; // Limiar de velocidade para considerar uma batida "forte"
const int LOW_VELOCITY_DISCARD_THRESHOLD = 29; // Limiar de velocidade para descartar batidas "fracas"
const unsigned long CROSSTALK_WINDOW_MS = 130; // Janela de tempo após uma batida forte para ignorar batidas fracas

// Constante para o multiplicador mínimo do retrigger, para evitar toque duplo em toques muito baixos.
const float RETRIGGER_MIN_MULTIPLIER = 1.5;

// --- Variáveis de Estado para o Pedal do Chimbal ---
int pedalChimbalState = HIGH;
int lastPedalChimbalState = HIGH;
bool chimbalOpenSoundPlaying = false;
bool chimbalClosedSoundPlaying = false;

// NOVO: Flags para controlar as notas dual-zone sendo tocadas (para o choke)
bool conducaoBordaPlaying = false;
bool conducaoCupulaPlaying = false;
bool ataqueBordaPlaying = false;
bool ataqueCupulaPlaying = false;
bool caixaPlaying = false; // Para a Caixa (não tem choke, mas é bom ter o controle)
bool aroCaixaPlaying = false; // Para o Aro da Caixa

// --- Nova variável para armazenar o retrigger inicial do decaimento ---
int retriggerThresholdInitialDecay[NUM_PADS];

void setup() {
  Serial.begin(31250); // Para Arduino Leonardo, esta linha configura a comunicação serial USB para MIDI
  for (int i = 0; i < NUM_PADS; i++) {
    pinMode(piezoPin[i], INPUT);
    padState[i] = PAD_STATE_IDLE; // Inicializa todos os pads no estado IDLE
    stateChangeTime[i] = 0;
    peakFoundTime[i] = 0;
    retriggerThresholdInitialDecay[i] = 0; // Inicializa a nova variável
  }

  pinMode(PEDAL_CHIMBAL_PIN, INPUT_PULLUP);
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Processamento do Pedal do Chimbal (Digital) ---
  int currentPedalReading = digitalRead(PEDAL_CHIMBAL_PIN);
  if (currentPedalReading != lastPedalChimbalState) {
    lastPedalChimbalState = currentPedalReading;
    if (currentPedalReading == LOW) { // Pedal pressionado (Fechado)
      if (chimbalOpenSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0);
        chimbalOpenSoundPlaying = false;
      }
      // midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0); // Esta linha era duplicada, removi.
      midiNoteOn(MIDI_NOTE_CHIMBAL_PEDAL, 30);
    } else { // Pedal Solto (Aberto)
      if (chimbalClosedSoundPlaying) {
        midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0);
        chimbalClosedSoundPlaying = false;
      }
    }
  }
  pedalChimbalState = currentPedalReading;

  // --- Processamento dos Pads Simples (Primeiro Loop) ---
  for (int j = 0; j <= CHIMBAL_PAD; j++) {
    int currentSensorReading = analogRead(piezoPin[j]); // Sempre lê o sensor
    currentMillis = millis(); // Atualiza o tempo

    switch (padState[j]) {
      case PAD_STATE_IDLE:
        if (currentSensorReading > threshold[j]) {
          // Detectou um toque inicial, inicia a detecção de pico
          peakSensorValues[j] = currentSensorReading;
          peakFoundTime[j] = currentMillis;
          padState[j] = PAD_STATE_PEAK_DETECTION;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_PEAK_DETECTION:
        // Continua buscando o pico dentro da janela
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorReading > peakSensorValues[j]) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
          }
        } else {
          // Janela de detecção de pico encerrou.
          if (peakSensorValues[j] <= threshold[j]) {
            padState[j] = PAD_STATE_IDLE;
          } else {
            // Sinal validado pela detecção de pico.
            int sensorValueAdjusted = round(peakSensorValues[j] * gainFactor[j]);
            int velocity = map(sensorValueAdjusted, threshold[j], 1023, minVelocity, maxVelocity);
            velocity = constrain(velocity, minVelocity, maxVelocity);

            // Lógica: Eliminação de Crosstalk baseada na velocidade e tempo
            if (velocity < LOW_VELOCITY_DISCARD_THRESHOLD &&
                (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
                // Esta leitura é uma batida fraca dentro da janela de crosstalk, ignora
                padState[j] = PAD_STATE_IDLE;
            } else {
                // Se não for crosstalk, envia a nota MIDI
                if (j == CHIMBAL_PAD) {
                  if (pedalChimbalState == LOW) { // Pedal Fechado: Toca chimbal fechado
                    midiNoteOn(MIDI_NOTE_CHIMBAL_CLOSED, velocity);
                    chimbalClosedSoundPlaying = true;
                    if (chimbalOpenSoundPlaying) {
                      midiNoteOff(MIDI_NOTE_CHIMBAL_OPEN, 0);
                      chimbalOpenSoundPlaying = false;
                    }
                  } else { // Pedal Solto: Toca chimbal aberto
                    midiNoteOn(MIDI_NOTE_CHIMBAL_OPEN, velocity);
                    chimbalOpenSoundPlaying = true;
                    if (chimbalClosedSoundPlaying) {
                      midiNoteOff(MIDI_NOTE_CHIMBAL_CLOSED, 0);
                      chimbalClosedSoundPlaying = false;
                    }
                  }
                } else {
                  midiNoteOn(midiNote[j], velocity);
                }

                // Atualiza o tempo da última batida forte se a velocidade for alta
                if (velocity > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                    lastHighVelocityMidiTime = currentMillis;
                }

                // Transiciona para o debounce silencioso após disparar a nota
                padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                stateChangeTime[j] = currentMillis;

                // Armazena o valor inicial do retrigger para o decaimento linear
                retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], peakSensorValues[j] * RETRIGGER_MIN_MULTIPLIER));
            }
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
        // Verifica se o tempo do debounce silencioso já passou
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          // Transiciona para o estado de checagem de repique
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE; // Retorna ao estado ocioso se o tempo de repique acabou
        } else {
          long currentRetriggerThreshold;
          currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
          currentRetriggerThreshold = max((long)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), currentRetriggerThreshold);

          if (currentSensorReading > currentRetriggerThreshold) {
            peakSensorValues[j] = currentSensorReading;
            peakFoundTime[j] = currentMillis;
            padState[j] = PAD_STATE_PEAK_DETECTION;
            stateChangeTime[j] = currentMillis;
          }
        }
        break;
      // O estado CHOKE_CONFIRMATION não é aplicável a pads simples, então não há case aqui.
    }
  }

  // --- Processamento de pads dual-zone (Segundo Loop) ---
  for (int j = CAIXA_PAD; j < NUM_PADS; j = j + 2) {
    int currentSensorPrincipalReading = analogRead(piezoPin[j]);
    int currentSensorSecundarioReading = analogRead(piezoPin[j+1]);
    currentMillis = millis(); // Atualiza o tempo

    switch (padState[j]) {
      case PAD_STATE_IDLE:
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
        // Continua buscando o pico dentro da janela
        if (currentMillis - stateChangeTime[j] < PEAK_DETECTION_WINDOW_MS) {
          if (currentSensorPrincipalReading > peakSensorValues[j]) {
            peakSensorValues[j] = currentSensorPrincipalReading;
            peakFoundTime[j] = currentMillis;
          }
          if (currentSensorSecundarioReading > peakSensorValues[j+1]) {
            peakSensorValues[j+1] = currentSensorSecundarioReading;
            peakFoundTime[j+1] = currentMillis;
          }
        } else {
          // Janela de detecção de pico encerrou.
          if (peakSensorValues[j] <= threshold[j] && peakSensorValues[j+1] <= threshold[j+1]) {
            padState[j] = PAD_STATE_IDLE;
          } else {
            // Sinal validado pela detecção de pico.
            int sensorValueAdjusted = round(peakSensorValues[j] * gainFactor[j]);
            int velocityPrincipal = map(sensorValueAdjusted, threshold[j], 1023, minVelocity, maxVelocity);
            velocityPrincipal = constrain(velocityPrincipal, minVelocity, maxVelocity);

            sensorValueAdjusted = round(peakSensorValues[j+1] * gainFactor[j+1]);
            int velocitySecundario = map(sensorValueAdjusted, threshold[j+1], 1023, minVelocity, maxVelocity);
            velocitySecundario = constrain(velocitySecundario, minVelocity, maxVelocity);

            // Lógica: Eliminação de Crosstalk baseada na velocidade e tempo
            if (max(velocitySecundario, velocityPrincipal) < LOW_VELOCITY_DISCARD_THRESHOLD &&
                (currentMillis - lastHighVelocityMidiTime < CROSSTALK_WINDOW_MS)) {
                padState[j] = PAD_STATE_IDLE;
            } else {
                // Se não for crosstalk, envia a nota MIDI
                // *** LÓGICA DE RIMSHOT PARA CAIXA_PAD ***
                if (j == CAIXA_PAD) {
                  // Condição de Rimshot: Ambos os sensores da caixa/aro detectam um golpe forte e simultâneo
                  // Os valores de 600 são sugestões para "golpe forte" e podem ser ajustados.
                  // Estou usando 600 como base, pois está próximo do retriggerThreshold da caixa (550).
                  if (peakSensorValues[CAIXA_PAD] > 600 && peakSensorValues[ARO_CAIXA_PAD] > 2*threshold[ARO_CAIXA_PAD]) {
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
                }
                // *** LÓGICA PARA CONDUÇÃO E ATAQUE (COM CHOKE CONFIRMATION) ***
                else { // Para CONDUCAO_BORDA_PAD e ATAQUE_BORDA_PAD
                    if (peakSensorValues[j] < 1000 && peakSensorValues[j+1] > peakSensorValues[j]) { // som de cupula
                      midiNoteOn(midiNote[j+1], velocitySecundario);
                    }
                    // NOVO: Transição para o estado de confirmação de choke para Condução e Ataque
                    else if (peakSensorValues[j+1] < peakSensorValues[j] * 0.05) { // Potencial choke
                      //padState[j] = PAD_STATE_CHOKE_CONFIRMATION; tá bugado
                      
                      midiNoteOff(midiNote[j], 0);
                      midiNoteOff(midiNote[j+1], 0);
                      stateChangeTime[j] = currentMillis; // Marca o início do período de confirmação
                    }
                    else { // borda maior (não é choke e não é cúpula)
                      midiNoteOn(midiNote[j], velocityPrincipal);
                    }
                }

                // Atualiza o tempo da última batida forte se a velocidade for alta
                if (max(velocitySecundario, velocityPrincipal) > HIGH_VELOCITY_THRESHOLD && midiNote[j] > 36) {
                    lastHighVelocityMidiTime = currentMillis;
                }

                // Transiciona para o debounce silencioso após disparar a nota
                // APENAS SE NÃO ENTROU NO CHOKE_CONFIRMATION (para Condução/Ataque)
                if (padState[j] != PAD_STATE_CHOKE_CONFIRMATION) {
                    padState[j] = PAD_STATE_SILENT_DEBOUNCE;
                    stateChangeTime[j] = currentMillis;
                }

                // Armazena o valor inicial do retrigger para o decaimento linear
                retriggerThresholdInitialDecay[j] = max((int)(threshold[j] * RETRIGGER_MIN_MULTIPLIER), min(retriggerThreshold[j], max(peakSensorValues[j], peakSensorValues[j+1]) * RETRIGGER_MIN_MULTIPLIER));
                retriggerThresholdInitialDecay[j+1] = retriggerThresholdInitialDecay[j]; // Aplica ao secundário também
            }
          }
        }
        break;

      case PAD_STATE_SILENT_DEBOUNCE:
        // Verifica se o tempo do debounce silencioso já passou
        if (currentMillis - stateChangeTime[j] >= SILENT_DEBOUNCE_MS) {
          // Transiciona para o estado de checagem de repique
          padState[j] = PAD_STATE_REPIQUE_CHECK;
          stateChangeTime[j] = currentMillis;
        }
        break;

      case PAD_STATE_REPIQUE_CHECK:
        unsigned long elapsedTime = currentMillis - stateChangeTime[j];
        if (elapsedTime >= REPIQUE_CHECK_MS) {
          padState[j] = PAD_STATE_IDLE;
        } else {
          long currentRetriggerThreshold;
          currentRetriggerThreshold = map(elapsedTime, 0, REPIQUE_CHECK_MS, retriggerThresholdInitialDecay[j], threshold[j]);
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

       case PAD_STATE_CHOKE_CONFIRMATION: // Estado de Confirmação de Choke (apenas para Condução e Ataque)
        // Certifica-se de que este estado só seja processado para pads de Condução e Ataque
        if (j == CONDUCAO_BORDA_PAD || j == ATAQUE_BORDA_PAD) {
          int currentPrimaryRead = analogRead(piezoPin[j]);
          int currentSecondaryRead = analogRead(piezoPin[j+1]);

          // Continuar buscando o pico dos dois sensores durante o tempo de confirmação
          if (currentPrimaryRead > peakSensorValues[j]) {
            peakSensorValues[j] = currentPrimaryRead;
          }
          if (currentSecondaryRead > peakSensorValues[j+1]) {
            peakSensorValues[j+1] = currentSecondaryRead;
          }

          // Se o tempo de confirmação passou
          if (currentMillis - stateChangeTime[j] >= CHOKE_CONFIRMATION_TIME_MS) {
            // Reavalia o estado do sensor secundário usando os picos finais para confirmar o choke
            // NOTA: peakSensorValues[j] aqui é o pico máximo atingido no primário durante a janela de choke
            //       peakSensorValues[j+1] aqui é o pico máximo atingido no secundário durante a janela de choke
            if (peakSensorValues[j+1] < (peakSensorValues[j] * 0.05) || peakSensorValues[j+1] < 20) {
              // Choke confirmado! Enviar MIDI Note Offs para as notas que estão tocando neste pad
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
}

// --- Funções MIDI ---
void midiNoteOn(int note, int velocity) {
  byte channel = 0;
  Serial.write(0x90 | channel);
  Serial.write((byte)note);
  Serial.write((byte)velocity);

  // Atualiza flags de notas tocando (para choke)
  if (note == midiNote[CONDUCAO_BORDA_PAD]) conducaoBordaPlaying = true;
  else if (note == midiNote[CONDUCAO_CUPULA_PAD]) conducaoCupulaPlaying = true;
  else if (note == midiNote[ATAQUE_BORDA_PAD]) ataqueBordaPlaying = true;
  else if (note == midiNote[ATAQUE_CUPULA_PAD]) ataqueCupulaPlaying = true;
  else if (note == midiNote[CAIXA_PAD]) caixaPlaying = true;
  else if (note == midiNote[ARO_CAIXA_PAD]) aroCaixaPlaying = true;
  else if (note == MIDI_NOTE_CHIMBAL_CLOSED) chimbalClosedSoundPlaying = true;
  else if (note == MIDI_NOTE_CHIMBAL_OPEN) chimbalOpenSoundPlaying = true;
}

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
