TODO: Agregar el informe al repo

# POP3 PROXY
## Protocolos de comunicación

### Indice de documentos

- `Informe.pdf`: El informe del proyecto se encuentra en el archivo
- `src/`: Directorio que contiene todos los archivos fuente.
- `mime/`: Directorio que contiene distintos archivos de prueba para el transformador de mensajes.

### Uso del software

#### Generación de ejecutables

1. Clonar el repositorio:
   ```bash  
   git clone https://bitbucket.org/itba/pc-2017b-01.git  
   ```

2. Ingresar a la carpeta `src`:
   ```bash  
   cd pc-2017b-01/src  
   ```

3. Compilar los archivos fuente:
   ```bash  
   make  
   ```

Esto generará los archivos `pop3filter`, `pop3ctl` y `stripmime` en el directorio `src`.

#### Proxy
Estando en el directorio `src`, se utiliza el siguiente comando para iniciar el proxy de POP3:

```bash
./pop3filter -p1212 -L 127.0.0.1 -m 'Mensaje de reemplazo' -M 'text/plain' ::1
```

#### Cliente de configuración
Estando en el directorio `src`, se utiliza el siguiente comando para correr el cliente de configuración y activar las métricas de conexiones concurrentes y accesos históricos, y luego obtenerlas:

```bash
./pop3ctl 127.0.0.1 9090 -4 -5 -1 -2
```

#### Transformador de mensajes
Estando en el directorio `src`, se pueden correr todos los casos de prueba de la siguiente manera:

##### Configuración previa
1. Configurar variables de entorno:
   ```bash  
   export CLIENT_NUM=100 && export FILTER_MEDIAS="images/png" && export FILTER_MSG="Parte reemplazada." && export POP3FILTER_VERSION=0.0 && export POP3_USERNAME=foo && export POP3_SERVER=bar  
   ```

2. Compilar transformador de mensajes
   ```bash  
   make stripmime  
   ```

##### Ejecución de casos de prueba

- 100 - Inmutabilidad.

```bash
cp ../mime/mensajes/ii_images.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="application/json" ./stripmime && diff retr_mail_0100 resp_mail_0100
```

- 101 - Standalone: Simple PNG.

```bash
cp ../mime/mensajes/ii_images.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="image/png" ./stripmime; diff retr_mail_0100 resp_mail_0100
```

-102 - Standalone: PNG y JPEG, lista.

```
cp ../mime/mensajes/ii_images.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="image/png,image/jpeg" ./stripmime && vim resp_mail_0100
```

- 103 - Standalone: PNG y JPEG: wildcard.

```
cp ../mime/mensajes/ii_images.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="image/*" ./stripmime && vim resp_mail_0100
```

- 104 - Anidado PNG y JPEG: wildcard.

```
cp ../mime/mensajes/iii_images_fwd.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="image/*" ./stripmime && vim resp_mail_0100
```

- 105 - Correo grande: Inmutabilidad

```
cp ../mime/mensajes/i_big.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="application/json" ./stripmime; diff retr_mail_0100 resp_mail_0100
```
- 106 - Correo grande: Mutabilidad

```
cp ../mime/mensajes/i_big.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MSG="Aqui estaba el archivo" FILTER_MEDIAS=application/x-cd-image ./stripmime
```

### Documentación de opciones

#### Proxy

TODO: Completar

#### Cliente de configuración

TODO: Completar

### Documentos utilizados como soporte durante la presentacion
TODO: Completar
