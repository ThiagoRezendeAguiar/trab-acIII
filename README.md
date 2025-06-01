# Simulador do Algoritmo de Tomasulo

Este projeto implementa um simulador completo do algoritmo de Tomasulo para execução dinâmica de instruções, desenvolvido em C++. O simulador suporta execução fora de ordem (out-of-order execution) com renomeação de registradores usando o Reorder Buffer (ROB).

## Grupo
- Felipe Lacerda Tertuliano
- Henrique Resende Lara
- Luigi Louback de Oliveira
- Thiago Rezende Aguiar
- Tiago Lascasas Antunes

## Visão Geral

O algoritmo de Tomasulo permite execução fora de ordem de instruções, resolvendo dependências de dados dinamicamente e maximizando o paralelismo de instruções. Esta implementação inclui:

- **Reservation Stations** para diferentes tipos de operações
- **Reorder Buffer (ROB)** para commit em ordem
- **Renomeação de registradores** para eliminar dependências WAR e WAW
- **Broadcast de resultados** via Common Data Bus (CDB)

## Arquitetura do Simulador

### Componentes Principais

#### 1. Reservation Stations
O simulador possui três tipos de estações de reserva:

- **ADD/SUB Stations**: Para operações de adição e subtração
- **MUL/DIV Stations**: Para operações de multiplicação e divisão  
- **LOAD/STORE Stations**: Para operações de memória

Cada estação contém:
- `name`: Nome da estação (ADD1, MUL1, LOAD1, etc.)
- `op`: Operação sendo executada
- `vj, vk`: Valores dos operandos
- `qj, qk`: Tags do ROB para operandos pendentes
- `destRobTag`: Tag do ROB de destino
- `remainingCycles`: Ciclos restantes para execução

#### 2. Reorder Buffer (ROB)
Buffer circular que mantém instruções em ordem de programa:

- `busy`: Indica se a entrada está ocupada
- `ready`: Indica se a instrução foi executada
- `destination`: Registrador ou endereço de destino
- `value`: Resultado da instrução
- `type`: Tipo de instrução (arith, load, store)
- `instructionIndex`: Índice da instrução

#### 3. Registradores
Cada registrador mantém:
- `value`: Valor atual
- `robTag`: Tag do ROB que produzirá o próximo valor (-1 se disponível)

### Latências de Execução

```cpp
static const int ADD_SUB_LATENCY = 2;    // Adição/Subtração: 2 ciclos
static const int MUL_LATENCY = 10;       // Multiplicação: 10 ciclos  
static const int DIV_LATENCY = 40;       // Divisão: 40 ciclos
static const int LOAD_STORE_LATENCY = 2; // Load/Store: 2 ciclos
```

## Instruções Suportadas

### Instruções Aritméticas
- `ADD rd rs1 rs2`: rd = rs1 + rs2
- `SUB rd rs1 rs2`: rd = rs1 - rs2
- `MUL rd rs1 rs2`: rd = rs1 * rs2
- `DIV rd rs1 rs2`: rd = rs1 / rs2

### Instruções de Memória
- `LW rd rs1 imm`: rd = Memory[rs1 + imm]
- `SW rs2 rs1 imm`: Memory[rs1 + imm] = rs2

## Pipeline de Execução

O simulador implementa um pipeline de 4 estágios:

### 1. Issue (Emissão)
- Aloca entrada no ROB
- Aloca estação de reserva
- Lê operandos ou configura dependências
- Renomeia registrador de destino

### 2. Execute (Execução)
- Aguarda operandos ficarem disponíveis
- Executa operação por N ciclos (conforme latência)
- Calcula resultado

### 3. Write Result (Escrita de Resultado)
- Escreve resultado no ROB
- Faz broadcast via CDB para estações dependentes
- Atualiza dependências

### 4. Commit (Confirmação)
- Instruções confirmam em ordem de programa
- Atualiza registradores/memória
- Libera entrada do ROB

## Estrutura de Classes

### Classe Principal: `Tomasulo`

#### Membros Privados
```cpp
queue<Instruction> instructions;           // Fila de instruções
vector<InstructionStatus> instructionsStatus; // Status de cada instrução
vector<ReservationStation> addRS;          // Estações ADD/SUB
vector<ReservationStation> mulRS;          // Estações MUL/DIV  
vector<ReservationStation> loadStoreRS;    // Estações LOAD/STORE
vector<ROBEntry> rob;                      // Reorder Buffer
unordered_map<string, Register> registers; // Banco de registradores
unordered_map<string, int> memory;         // Memória
```

#### Métodos Principais
- `loadInstructions()`: Carrega instruções de arquivo
- `run()`: Loop principal de simulação
- `issueInstruction()`: Emite próxima instrução
- `executeInstructions()`: Executa instruções nas estações
- `writeResults()`: Escreve resultados no ROB
- `commit()`: Confirma instruções em ordem
- `broadcastResult()`: Propaga resultados via CDB

## Uso do Simulador

### Compilação
```bash
g++ -o tomasulo tomasulo.cpp -std=c++11
```

### Execução
```bash
./tomasulo
```

O simulador solicitará o caminho do arquivo de instruções.

### Formato do Arquivo de Instruções
```
ADD R1 R2 R3
SUB R4 R1 R5
MUL R6 R4 R2
LW R7 R1 100
SW R6 R2 200
```

### Configuração Inicial
O simulador pode ser configurado com:
- Número de estações de reserva por tipo
- Tamanho do ROB
- Valores iniciais de registradores e memória

```cpp
Tomasulo simulator(3, 2, 3, 6); // 3 ADD, 2 MUL, 3 LOAD, ROB=6
simulator.setRegister("R0", 5);
simulator.setMemory("105", 10);
```

## Saída do Simulador

### Durante Execução
Para cada ciclo, o simulador exibe:
- Estado das estações de reserva
- Estado do ROB
- Estado dos registradores

### Resultados Finais
- Timeline de execução de cada instrução
- Valores finais dos registradores
- Conteúdo final da memória

## Características Especiais

### Resolução de Hazards
- **RAW (Read After Write)**: Resolvido via estações de reserva e broadcast
- **WAR (Write After Read)**: Eliminado por renomeação de registradores
- **WAW (Write After Write)**: Eliminado por renomeação de registradores

## Limitações

- Não suporta instruções de branch/jump
- Não implementa predição de branch
- Tamanhos fixos para estruturas de dados
- Sem suporte a cache miss/hit

# Exemplo de Simulação

```
ADD R6 R0 R1
SUB R7 R6 R2
MUL R8 R7 R3
DIV R9 R8 R4
LW R10 R0 100
SW R10 R1 200
ADD R11 R10 R5
ADD R10 R2 R4
SUB R12 R11 R9
MUL R12 R10 R3
ADD R13 R12 R6
SUB R14 R13 R7
MUL R15 R14 R2
```

## Análise das Dependências

### 1. **Dependências RAW (Read After Write) - Verdadeiras**
- `SUB R7 R6 R2` depende de `ADD R6 R0 R1`
- `MUL R8 R7 R3` depende de `SUB R7 R6 R2`
- `DIV R9 R8 R4` depende de `MUL R8 R7 R3`
- `SW R10 R1 200` depende de `LW R10 R0 100`
- `ADD R11 R10 R5` depende de `LW R10 R0 100`
- `SUB R12 R11 R9` depende de `ADD R11 R10 R5` e `DIV R9 R8 R4`
- `MUL R12 R10 R3` depende de `ADD R10 R2 R4`
- `ADD R13 R12 R6` depende de `MUL R12 R10 R3`
- `SUB R14 R13 R7` depende de `ADD R13 R12 R6`
- `MUL R15 R14 R2` depende de `SUB R14 R13 R7`

### 2. **Dependências WAR (Write After Read) - Resolvidas por Renomeação**
- `ADD R10 R2 R4` vs `ADD R11 R10 R5` (R10 é lido antes de ser reescrito)

### 3. **Dependências WAW (Write After Write) - Resolvidas por Renomeação**
- `SUB R12 R11 R9` vs `MUL R12 R10 R3` (R12 é escrito duas vezes)

## Configuração Inicial
```
int main()
{
    Tomasulo simulator(3, 2, 3, 6);

    string filePath;
    cout << "Enter the file path:" << endl;
    cin >> filePath;
    cin.ignore();

    if (!simulator.loadInstructions(filePath))
    {
        cerr << "Error loading instructions.";
        return 1;
    }

    simulator.setRegister("R0", 5);   
    simulator.setRegister("R1", 3);   
    simulator.setRegister("R2", 2);   
    simulator.setRegister("R3", 3);   
    simulator.setRegister("R4", 2);  
    simulator.setRegister("R5", 5); 
    
    simulator.setMemory("105", 10); 
    simulator.setMemory("203", 0);  

    simulator.run();

    return 0;
}
```

## Valores Finais dos Registradores

```
R0 = 5    (valor inicial)
R1 = 3    (valor inicial)
R2 = 2    (valor inicial)
R3 = 3    (valor inicial)
R4 = 2    (valor inicial)
R5 = 5    (valor inicial)
R6 = 8    (5 + 3)
R7 = 6    (8 - 2)
R8 = 18   (6 * 3)
R9 = 9    (18 / 2)
R10 = 4   (2 + 2) - valor da segunda instrução ADD R10
R11 = 15  (10 + 5)
R12 = 12  (4 * 3) - valor da segunda instrução MUL R12
R13 = 20  (12 + 8)
R14 = 14  (20 - 6)
R15 = 28  (14 * 2)
```

## Valores Finais da Memória

```
Memory[105] = 10  (valor inicial, lido pelo LW)
Memory[203] = 10  (escrito pelo SW R10 R1 200)
```
