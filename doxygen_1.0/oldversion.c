/**
 * @defgroup LOOP_PADS_SIMPLES Processamento dos Pads Simples
 * @ingroup MAIN_LOOP
 * @brief Loop para a detecção e processamento de pads com uma única zona.
 * @details Cada pad simples passa por uma máquina de estados para garantir detecção precisa: @see PadState
 * @{
 */
/** @} */

/** @ingroup LOOP_PADS_SIMPLES */
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
