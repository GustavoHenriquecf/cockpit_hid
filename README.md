# Cockpit Caseiro — Controlador HID USB (Arduino Pro Micro)

Painel de controle físico para simuladores de voo (FSX / MSFS 2020), baseado em um **Arduino Pro Micro (ATmega32U4)** configurado como dispositivo **USB HID nativo**. O firmware transforma o Arduino em um joystick reconhecido diretamente pelo Windows, sem necessidade de drivers adicionais.

## Recursos

- **15 botões digitais** com detecção de mudança de estado e debounce por `millis()`
- **3 eixos analógicos** (Potência, Mistura e Passo de Hélice), com calibração de curso limitado e filtro de ruído (deadband)
- Biblioteca [Joystick](https://github.com/MHeironimus/ArduinoJoystickLibrary) de Matthew Heironimus, tipo `JOYSTICK_TYPE_JOYSTICK`
- Modo de calibração via Serial Monitor para descobrir os valores reais do curso de cada potenciômetro

## Hardware

| Componente | Detalhe |
|---|---|
| Placa | Arduino Pro Micro (ATmega32U4) |
| Botões | 15 chaves/botões momentâneos, fechando contato com GND comum |
| Manetes | 3 potenciômetros lineares (Potência, Mistura, Passo de Hélice) |

### Pinagem — Botões digitais

Todos configurados com `INPUT_PULLUP` (pressionado = `LOW`):

```
2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, A3, 1, 0, 16
```

### Pinagem — Eixos analógicos

| Manete | Pino | Eixo HID |
|---|---|---|
| Potência | A2 | Y Axis |
| Mistura | A1 | X Axis |
| Passo de Hélice | A0 | Z Axis |

> Os eixos usam `X`, `Y` e `Z` (página Generic Desktop) em vez de `Throttle`/`Rudder`/`Accelerator` (página Simulation Controls) porque o driver HID legado do Windows (`joy.cpl`) não traduz corretamente esses últimos — eles simplesmente não aparecem como eixos utilizáveis.

## Instalação

1. Instale a IDE do Arduino.
2. No **Gerenciador de Bibliotecas**, instale **Joystick by Matthew Heironimus**.
3. Selecione a placa **Arduino Leonardo** ou **SparkFun Pro Micro** (ambas usam o ATmega32U4 com HID nativo).
4. Grave o sketch `cockpit_hid.ino`.

## Calibração

As manetes deslizam em um trilho reto e **não percorrem o curso físico completo de 270°** dos potenciômetros — por isso a leitura bruta (`analogRead`) nunca chega a 0 ou 1023, e cada eixo tem seu próprio intervalo real.

### Como recalibrar

1. No topo do sketch, altere:
   ```cpp
   #define MODO_CALIBRACAO true
   ```
2. Grave o código e abra o **Serial Monitor** (9600 baud).
3. Mova cada manete lentamente do fim ao fim do curso físico, anotando o menor e o maior valor impresso para cada eixo.
4. Preencha as constantes correspondentes:
   ```cpp
   int potenciaMin = ...;
   int potenciaMax = ...;
   int misturaMin  = ...;
   int misturaMax  = ...;
   int heliceMin   = ...;
   int heliceMax   = ...;
   ```
5. Volte `MODO_CALIBRACAO` para `false` e grave novamente — o HID passa a funcionar normalmente, já calibrado.

### Valores atuais (referência desta build)

| Eixo | Mínimo | Máximo |
|---|---|---|
| Potência (Y) | 389 | 711 |
| Mistura (X) | 319 | 643 |
| Hélice (Z) | 256 | 590 |

Esses valores são específicos da mecânica desta caixa — **remeça sempre que trocar potenciômetro, trilho ou linkagem**.

## Configuração no MSFS 2020

1. Em **Opções > Controles**, selecione o dispositivo **Arduino Leonardo**.
2. Na aba **Eixos**, faça o bind por movimento: `THROTTLE1 AXIS` → Potência, `PROP PITCH1 AXIS` → Hélice, `MIXTURE1 AXIS` → Mistura.
3. Teste o curso de cada eixo na tela de Controles (deve ir de 0% a 100% nas pontas); ajuste sensibilidade/inversão se necessário, mantendo a curva linear.
4. Na aba **Botões**, mapeie as 15 entradas digitais para as funções desejadas do painel.
5. Salve como um perfil de controle dedicado (ex: "Cockpit Físico").

## Solução de problemas

| Sintoma | Causa provável |
|---|---|
| Eixo aparece com nome errado (ex: "Leme") no `joy.cpl` | Ordem incorreta dos parâmetros booleanos no construtor `Joystick_()` |
| Eixo simplesmente não aparece no `joy.cpl` | Uso de um eixo da página Simulation Controls (Throttle/Rudder/Accelerator) sem tradução no driver legado do Windows — use X/Y/Z/Rx/Ry/Rz |
| Eixo "salta" no início do curso | `Min` configurado acima da leitura bruta real — recalibre |
| Eixo satura em 100% antes do fim do curso físico | `Max` configurado abaixo da leitura bruta real — recalibre |
| Dispositivo não atualiza no Windows após regravar | Desinstale o dispositivo `HID\VID_2341&PID_8036&MI_02` no Gerenciador de Dispositivos e reconecte em outra porta USB |

