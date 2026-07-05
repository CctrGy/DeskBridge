# DeskBridge

DeskBridge es un proyecto de hardware y software pensado para crear un centro de control de escritorio, capaz de comunicarse con el ordenador, mostrar informacion en pantalla, gestionar botones fisicos, controlar iluminacion y reunir estados utiles del propio dispositivo.

La idea principal es que el dispositivo actue como un puente entre el PC y varios modulos fisicos. Desde el ordenador se puede consultar el estado del sistema, enviar comandos, recibir eventos, revisar avisos y preparar futuras herramientas de configuracion o interfaz grafica. Desde el hardware se puede interactuar mediante pantalla OLED, botones, encoder, indicadores luminosos y perifericos conectados.

## Vision General

DeskBridge busca convertirse en una plataforma modular. En lugar de ser un unico programa cerrado, el proyecto esta dividido en varias partes que pueden crecer de forma independiente:

- Un nucleo principal que coordina el sistema.
- Un modulo auxiliar dedicado a controles fisicos, teclado, encoder e iluminacion.
- Una aplicacion de PC para comunicarse con el dispositivo.
- Una interfaz de terminal avanzada para pruebas, diagnostico y control.
- Un sistema de notificaciones y eventos para informar al usuario de lo que ocurre.

El objetivo final es que el sistema sea facil de probar, ampliar y reprogramar sin tener que rehacer todo desde cero cada vez que se cambie una parte del hardware o de la logica.

## CHIP_CORE

El CHIP_CORE es el centro del sistema. Es el encargado de mantener la comunicacion principal con el ordenador, coordinar el estado del dispositivo y actuar como punto de union entre los demas modulos.

Sus responsabilidades principales son:

- Comunicarse con el programa del PC.
- Gestionar la informacion general del dispositivo.
- Recibir y responder comandos.
- Controlar la pantalla OLED.
- Supervisar el estado de los perifericos.
- Enviar avisos, eventos y datos al ordenador.
- Mantener informacion basica de sistema, energia, hora, sensores y estado interno.

Este chip se plantea como la pieza estable del proyecto: el lugar donde vive la logica central y desde donde se organiza el resto del ecosistema.

## CHIP_KEYPAD

El CHIP_KEYPAD es un modulo complementario orientado a la interaccion fisica. Su funcion es descargar trabajo del nucleo principal y encargarse de elementos como botones, encoder, acciones rapidas e iluminacion.

Este modulo puede actuar como una extension del sistema principal. Permite que el proyecto crezca sin saturar el CHIP_CORE con todas las tareas fisicas. Tambien facilita que el sistema sea mas limpio, porque separa la logica de control principal de la logica de entrada y respuesta fisica.

Sus funciones previstas incluyen:

- Lectura de botones.
- Gestion de encoder.
- Control de luces o tiras LED.
- Envio de eventos al CHIP_CORE.
- Estado propio del modulo.
- Respuesta a comandos de configuracion.

## Programa de PC

El programa de PC es la herramienta de comunicacion y control desde el ordenador. Permite enviar comandos al dispositivo, recibir respuestas, revisar estados y consultar informacion de diagnostico.

Actualmente esta orientado a terminal, pero esta preparado para crecer hacia un sistema mas modular, con configuracion, idiomas, paletas de color, logs y posibles interfaces futuras.

Sus objetivos son:

- Facilitar las pruebas del hardware.
- Permitir el control del dispositivo desde el ordenador.
- Mostrar informacion clara del estado del sistema.
- Guardar registros de actividad.
- Separar comandos de usuario, respuestas, eventos, errores y datos.
- Servir como base para futuras aplicaciones graficas o herramientas de configuracion.

## TUI

La TUI es una interfaz de terminal mas visual que organiza la informacion en zonas. Su objetivo es hacer mas comoda la supervision del dispositivo durante las pruebas fisicas y el desarrollo.

La pantalla del TUI separa:

- Datos recibidos directamente del dispositivo.
- Errores.
- Eventos.
- Informacion de enlace.
- Zona de comandos.
- Estado general del sistema.

Esto permite trabajar con el dispositivo sin perder de vista lo que esta ocurriendo en segundo plano.

## Pantalla OLED

La OLED forma parte de la experiencia fisica del dispositivo. Esta pensada para mostrar informacion local sin depender siempre del ordenador.

Puede servir para:

- Ver el estado general.
- Consultar energia y conexion.
- Mostrar informacion del CHIP_CORE.
- Mostrar estado del CHIP_KEYPAD.
- Navegar por menus.
- Ver avisos o estados rapidos.
- Preparar futuras opciones de configuracion directa desde el dispositivo.

La OLED convierte el proyecto en un dispositivo independiente y no solo en una placa conectada al PC.

## Notificaciones y Eventos

DeskBridge diferencia entre respuestas normales, eventos, errores, datos y notificaciones. Esto permite que el programa del PC entienda mejor que tipo de informacion esta recibiendo.

Las notificaciones estan pensadas para mostrar avisos importantes de forma minimalista. Por ejemplo, si un modulo deja de responder, el PC puede mostrar una linea corta en la zona de informacion y permitir consultar mas detalle cuando el usuario lo necesite.

Este sistema ayuda a mantener la interfaz limpia: no todo tiene que aparecer como error grande, pero tampoco se pierde informacion importante.

## Energia y USB

El proyecto contempla un dispositivo con alimentacion propia, uso de USB-C, bateria auxiliar y capacidad de adaptarse a distintos estados de energia.

El USB no solo sirve para comunicar datos con el ordenador. Tambien forma parte del sistema de energia, carga, estado de conexion y posibles funciones futuras relacionadas con reactivacion del equipo o deteccion del estado del puerto.

La idea es que el sistema pueda informar de su estado energetico y actuar de forma distinta segun la fuente disponible.

## Modularidad

Una parte importante del proyecto es convertir la logica en bloques reutilizables. Esto facilita que otra persona pueda cambiar comportamientos, ampliar funciones o reprogramar partes del sistema sin entender todo el proyecto de golpe.

La modularidad busca:

- Separar responsabilidades.
- Hacer pruebas mas faciles.
- Evitar mezclar la logica de PC con la logica de firmware.
- Reutilizar protocolos entre chips.
- Preparar el proyecto para futuras placas o revisiones de hardware.

## Objetivo del Proyecto

El objetivo final de DeskBridge es crear un dispositivo de escritorio personalizable, capaz de combinar control fisico, informacion visual, comunicacion con el ordenador, gestion de energia, notificaciones y expansion por modulos.

No se trata solo de hacer que una placa responda comandos. La intencion es construir una base ordenada para un producto propio: un sistema que pueda evolucionar desde prototipo hasta una version mas final, con hardware separado por funciones y software preparado para crecer.

## Estado Actual

El proyecto ya cuenta con una estructura inicial formada por:

- Firmware del CHIP_CORE.
- Firmware del CHIP_KEYPAD.
- Programa de PC.
- TUI de monitorizacion.
- Sistema de comandos.
- Sistema de logs.
- Configuracion persistente.
- Idiomas y paletas.
- Comunicacion entre PC y dispositivo.
- Eventos y notificaciones.
- Scripts auxiliares para limpiar o programar chips.

El siguiente paso natural es seguir puliendo la organizacion interna, probar la conectividad fisica entre placas y convertir cada parte estable en librerias reutilizables.
