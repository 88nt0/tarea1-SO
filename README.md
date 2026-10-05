# Planificador Dieciochero

Simulador y planificador de actividades para Tarea 1 de Sistemas Operativos (UDP), que ejecuta un plan de tareas con dependencias (DAG) usando procesos, pipes y señales.

## Funciones implementadas

- **Parseo de `plan.txt`**: lee el archivo línea por línea, separa cada campo por `:`, recorta espacios en blanco. Si el tiempo viene vacío, se genera un valor aleatorio entre 100 y 5000 ms. Si una línea referencia una dependencia que no existe en el plan, el programa reporta el error y termina.
- **Modelado del DAG**: cada actividad se guarda con su grado de entrada (cantidad de dependencias pendientes) y su lista de dependientes (quién depende de ella). Antes de ejecutar nada, se valida que el plan no tenga ciclos usando el algoritmo de Kahn — si los tiene, el programa rechaza el plan.
- **Creación de procesos con límite K**: el planificador mantiene una cola de actividades "listas" (sin dependencias pendientes) y lanza procesos con `fork()` mientras haya cupo (menos de K corriendo) y actividades disponibles.
- **Concurrencia sin busy-waiting**: el proceso padre se bloquea con `waitpid(-1, ...)` esperando a que cualquier hijo termine, en vez de consultar su estado en un loop activo. Esto evita el uso innecesario de CPU y race conditions al actualizar el estado de las actividades, porque las actualizaciones solo ocurren cuando `waitpid` efectivamente retorna.
- **Paso de mensajes (pipes)**: antes de cada `fork()`, el padre crea un pipe. El hijo recibe por ahí un mensaje con los nombres de sus dependencias ya completadas (el "insumo"), simulando la entrega de resultados entre actividades.
- **Aislamiento de errores**: cada actividad tiene una probabilidad baja (5%) de fallar al azar. Si una actividad falla, el padre detecta el código de salida distinto de cero y aborta (sin ejecutar) a todos sus descendientes directos e indirectos, recorriendo el DAG en anchura. Las ramas del plan que no dependen de la actividad fallida siguen ejecutándose con normalidad, y el programa nunca se cierra por esto.
- **Manejo de SIGINT (Ctrl+C)**: al presionar Ctrl+C, un handler simple marca una variable global (`volatile sig_atomic_t`). El loop principal detecta la marca, deja de lanzar nuevas actividades, envía `SIGTERM` a todos los hijos vivos, los espera a todos con `waitpid` y termina mostrando un resumen de cuántas actividades alcanzaron a completarse.

## Compilación y ejecución

Compilar:
```bash
make
```

Ejecutar:
```bash
./planificador plan.txt K
```
Donde `plan.txt` es el archivo con las actividades y `K` es el número máximo de procesos corriendo al mismo tiempo.

Ejemplo:
```bash
./planificador tests/plan.txt 2
```

Limpiar el binario compilado:
```bash
make clean
```

## Decisiones de diseño

- **Definición de K**: se interpretó como el número máximo de procesos hijos vivos simultáneamente, no el total de procesos lanzados en toda la ejecución.
- **IDs de actividades**: se trataron como `string` en vez de números, porque el enunciado los define como alfanuméricos y el ejemplo del formato usa letras en la explicación.
- **Simulación de fallos**: se usó una probabilidad fija de 5% por actividad (`rand() % 100 < 5`), evaluada dentro del proceso hijo, después de recibir el mensaje por pipe y antes de ejecutar el trabajo simulado. Se eligió un valor bajo para que el aislamiento de errores sea visible en ejecuciones largas sin afectar demasiado los resultados en planes chicos.
- **Esquema de pipes**: se usa un pipe por actividad, creado antes del `fork()` correspondiente. El padre escribe el mensaje y cierra su extremo; el hijo lee y cierra el suyo. Cada proceso cierra el extremo del pipe que no le corresponde para evitar quedar esperando datos que nunca llegarán.
- **Espera bloqueante**: se optó por `waitpid(-1, &status, 0)` en vez de manejar `SIGCHLD` con un handler asíncrono, porque cumple igualmente con el requisito de no hacer busy-waiting y simplifica el control de race conditions, al no haber actualizaciones de estado desde un contexto de señal.
- **Aborto de ramas fallidas**: se usa un recorrido en anchura (BFS) desde la actividad fallida hacia sus dependientes, marcando cada una como abortada y sumándolas al contador de actividades "terminadas" (para que el programa no quede esperando indefinidamente actividades que nunca se van a ejecutar).

## Pruebas realizadas

- Plan de ejemplo del enunciado (6 actividades), verificando el orden de ejecución respeta las dependencias.
- Plan con ciclo intencional, verificando que el programa lo detecta y rechaza antes de ejecutar nada.
- Prueba de carga con 10000 actividades generadas aleatoriamente (sin ciclos), con distintos valores de K, verificando tiempos de ejecución razonables y ausencia de procesos zombie al finalizar (`ps aux`).
- Prueba manual de Ctrl+C durante la ejecución, verificando que el programa aborta limpiamente, reapea todos los hijos y no deja procesos zombie.
