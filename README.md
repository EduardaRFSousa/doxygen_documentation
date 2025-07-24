# doxygen_documentation

Explicação:
- O arquivo main.c contém a sintaxe da biblioteca que os meninos usaram (doxygen);
- O arquivo code_with_comments é o código original do professor com comentários sobre os loops,
é preciso usar ele pra ter uma ideia da lógica dos escopos e não se complicar

# VISÃO GERAL
void loop(){
PROCESSAMENTO DO PEDAL DO CHIMBAL (DIGITAL)
PROCESSAMENTO DOS PADS SIMPLES (1º LOOP)
    case PAD_STATE_IDLE 
    case PAD_STATE_PEAK_DETECTION
        if -> eliminação do crosstalk
        else -> se não for crosstalk, envia a nota MIDI
    case PAD_STATE_SILENT_DEBOUNCE
    case PAD_STATE_REPIQUE_CHECK

PROCESSAMENTO DE PADS DUAL-ZONE (2º LOOP)
    case PAD_STATE_IDLE
    case PAD_STATE_PEAK_DETECTION
        if -> eliminação do crosstalk
        else -> se não for crosstalk, envia a nota MIDI
            >> lógica de rimshot para CAIXA
            >> lógica para CONDUCAO e ATAQUE (com choke confirmation)
    case PAD_STATE_SILENT_DEBOUNCE
    case PAD_STATE_REPIQUE_CHECK
    case PAD_STATE_CHOKE_CONFIRMATION
}

# TODOS OS PADS:
Pads Simples:
BUMBO
SURDO
TOM1
TOM2
CHIMBAL

Pads Dual-Zone:
CAIXA
ARO_CAIXA
CONDUCAO_BORDA
CONDUCAO_CUPULA
ATAQUE_BORDA
ATAQUE_CUPULA

(ATT.: IGNORE O TEXTO A SEGUIR, DÁ PRA FAZER TUDO COM IA)
# ANÁLISE DO GEMINI SOBRE AS TAGS:
No arquivo main.c, as seguintes tags Doxygen foram utilizadas:

    1. @file: Usado para documentar o arquivo em si (bateriaChokev3.c).
    2. @author: Indica o autor do código.
    3. @brief: Fornece uma breve descrição do arquivo, das definições, variáveis e funções.
    4. @version: Especifica a versão do firmware.
    5. @date: Indica a data da última modificação significativa.
    6. @defgroup: Cria um grupo de documentação para organizar definições relacionadas (ex: PAD_INDICES, PIN_DEFINITIONS).
    7. @{ e @}: Usados para delimitar os membros de um grupo de documentação.
    8. @param: Descreve os parâmetros das funções midiNoteOn e midiNoteOff.
    9. @return: Não explicitamente usado, mas a função setup e loop não retornam valor.
    10. @note: Adiciona notas explicativas, como nas funções midiNoteOn e midiNoteOff.
    11. @see: Utilizado no @brief do array piezoPin para referenciar o grupo @ref PAD_INDICES.

# Tags Doxygen que podem ser adicionadas ou aprimoradas (OPCIONAL, MAS É BOM LER):
@details: Embora o @brief seja usado, um @details poderia ser adicionado a várias seções (especialmente para setup() e loop(), e para os grupos) para fornecer descrições mais aprofundadas sobre a lógica ou o propósito de blocos de código complexos, como funcionamento dos estados da máquina e a eliminação de crosstalk.
@invariant: Para documentar invariantes de estado ou condições que sempre devem ser verdadeiras para arrays como padState, peakSensorValues, etc., especialmente dentro do loop(). Por exemplo, que peakSensorValues[j] nunca será menor que 0.
@pre e @post: Para as funções midiNoteOn e midiNoteOff, poderíamos especificar pré-condições (ex: Serial deve estar inicializado) e pós-condições (ex: a nota MIDI correspondente foi enviada e a flag ...Playing foi atualizada).

Exemplo para midiNoteOn:
    @pre O Arduino deve ter a comunicação Serial inicializada (ex: Serial.begin(31250);).
    @post Uma mensagem MIDI Note On é enviada pela porta serial e a flag '...Playing' correspondente é definida como 'true'.
    @warning ou @attention: Poderiam ser adicionados em seções onde ajustes de valores (ex: threshold, retriggerThreshold, gainFactor) são críticos e requerem atenção do usuário para calibração. Por exemplo, na definição de threshold:
    @warning Ajustes inadequados dos limiares podem causar toques fantasmas ou perda de sensibilidade.
    @todo e @bug: Embora não haja um bug explícito no código, a parte comentada // padState[j] = PAD_STATE_CHOKE_CONFIRMATION; tá bugado no code_with_comments.c indica que há algo pendente que poderia ser marcado com @todo ou @bug para futura correção na documentação.

Exemplo:
    @todo Revisar e corrigir a transição para PAD_STATE_CHOKE_CONFIRMATION no loop de pads dual-zone, pois está desabilitada devido a um bug.
    @sa ou @see para funções e estados: Seria útil para interconectar a documentação. Por exemplo, na descrição de PAD_STATE_IDLE, referenciar outras funções ou constantes que afetam a transição para ou deste estado.
    @code e @endcode: Para incluir pequenos snippets de código nos comentários, se necessário, para ilustrar um conceito ou uso. Atualmente não há necessidade, mas é uma tag útil.
