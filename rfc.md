## RFC-UNA-2026-PULSE

```
Protocolo PULSE para Comunicación en Anillo de Luz
Kristel Montoya Chaves y Matteo Vargas Quesada
```
**1. Capa física**
    **a. Codificación de Bits: Manchester**
       Cada bit se transmite usando codificación Manchester diferencial:
          - Bit '1': transición ALTO → BAJO en la mitad del período de bit.
          - Bit '0': transición BAJO → ALTO en la mitad del período de bit.
          - Período total de bit: 50 ms (25 ms por semiciclo).
          - ALTO = láser encendido; BAJO = láser apagado.
    **b. Duración de Bit**
       - Período de bit (T_bit): 50 ms
       - Semiciclo (T_half): 25 ms
       - Tolerancia admitida: ±10% (±5 ms)
    **c. Sincronización**
       El receptor detecta el inicio de una trama mediante un pulso de sincronía previo al
       preámbulo:
          - Pulso START: láser encendido durante 150 ms, seguido de 50 ms apagado.
          - El receptor monitorea continuamente el fotoresistor buscando señal sostenida ≥
             130 ms.
          - Al detectar el pulso START, activa el muestreo Manchester cada 25 ms en la
             mitad del semiciclo.
          - Si no se recibe el byte PREÁMBULO (0xAA) válido tras el START, se descarta y
             reinicia.
**2. Capa superior 1: Enlace (Trama PULSE)**
    **a. Formato de Trama**
       Orden de transmisión: MSB primero. Todos los campos en big-endian.
    **b. Direccionamiento**
    - Cada nodo tiene un ID único de 4 bits asignado manualmente (0x1, 0x2, 0x3...).
    - Un nodo acepta la trama si ID_DESTINO coincide con su propio ID, o si es 0xF
       (broadcast).
    - Si ID_DESTINO no coincide, el nodo reenvía la trama íntegra al siguiente nodo del
       anillo.
**Campo Tamaño Descripción**
PREÁMBULO 8 bits Secuencia fija 0xAA (10101010). Indica inicio de trama.
ID_ORIGEN 4 bits Identificador del nodo emisor (0–15).
ID_DESTINO 4 bits Identificador del nodo receptor (0–15). 0xF = broadcast.
SEQ 4 bits Número de secuencia del mensaje (0–15).
LONGITUD 8 bits Número de bytes de datos cifrados en el payload (1–16).
PAYLOAD N×8 bits Datos cifrados con XOR 0x42. Longitud variable.
CHECKSUM 8 bits XOR acumulado de todos los campos excepto PREÁMBULO.
FIN 8 bits Secuencia fija 0x55 (01010101). Indica fin de trama.


```
c. Detección de Errores: Checksum XOR
```
- El campo CHECKSUM es el XOR byte a byte de: ID_ORIGEN, ID_DESTINO, SEQ,
    LONGITUD y todos los bytes de PAYLOAD.
- El receptor calcula el mismo XOR y compara. Si difieren, la trama se descarta
    silenciosamente.
**3. Capa superior 2: Seguridad (Cifrado XOR)
a. Algoritmo**
Cifrado XOR simétrico con clave fija de 1 byte .Se aplica byte a byte sobre el payload en
la capa de Seguridad, antes de construir la trama de Enlace.
- Clave: K = 0x42 (carácter 'B' en ASCII, binario: 01000010)
**b. Proceso de Cifrado (Emisor)**
- Paso 1: Obtener el mensaje original en bytes ASCII.
- Paso 2: Para cada byte M[i] del mensaje, calcular: C[i] = M[i] XOR 0x
- Paso 3: El arreglo C[] se coloca en el campo PAYLOAD de la trama.
- Paso 4: Calcular CHECKSUM sobre la trama ya con datos cifrados.
**c. Proceso de Descifrado (Receptor)**
- Paso 1: Verificar CHECKSUM. Si falla, descartar.
- Paso 2: Para cada byte C[i] del PAYLOAD, calcular: M[i] = C[i] XOR 0x
- Paso 3: Interpretar M[] como texto ASCII. El resultado es el mensaje original.
_Nota: XOR es auto-inverso. La misma operación cifra y descifra con la misma clave._
**d. Distribución de Clave**
La clave K = 0x42 es pre-compartida fuera de banda (acordada previamente entre los
grupos). No se transmite en ningún campo de la trama.
**4. Ejemplo de transmisión: Mensaje 'A'**
Mensaje original: 'A' → ASCII: 0x41 → Binario: 01000001
**a. Capa de Seguridad: Cifrado**
- M = 0x41 = 01000001
- K = 0x42 = 01000010
- C = M XOR K = 01000001 XOR 01000010 = 00000011 = 0x
El payload cifrado es: [0x03]
**b. Capa de Enlace: Construcción de Trama**
PREÁMBULO : 0xAA = 10101010
ID_ORIGEN : 0x1 (Nodo 1)
ID_DESTINO: 0x2 (Nodo 2)
SEQ : 0x0 (primera trama)
LONGITUD : 0x01 (1 byte de payload)
PAYLOAD : 0x03 (dato cifrado)
CHECKSUM : 0x1 XOR 0x2 XOR 0x0 XOR 0x01 XOR 0x03 = 0x
FIN : 0x55 = 01010101
**c. Capa Física: Pulso START + Transmisión**
- START : láser ON por 150 ms, luego OFF por 50 ms
- Trama : 0xAA 0x12 0x01 0x03 0x01 0x55 (Manchester, 50ms/bit)
El receptor verá 0x03 en el monitor serial antes de descifrar. Tras aplicar XOR 0x42: 0x03 XOR 0x42 =
0x41 = 'A'. Mensaje recuperado.

