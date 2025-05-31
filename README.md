# Resultado Esperado da Simulação

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
