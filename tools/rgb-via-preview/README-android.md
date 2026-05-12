# Android / PWA Setup

## PWA lokal testen

```bash
cd tools/rgb-via-preview
npm install
npm run dev
```

Im Browser kannst du die App dann installieren.

## Android vorbereiten

Benötigt:

- Android Studio
- Android SDK
- Java JDK
- Node.js

## Android-Projekt erzeugen

```bash
npm run build
npm run android:init
```

Dadurch entsteht:

```txt
android/
```

## Änderungen synchronisieren

```bash
npm run build
npm run android:sync
```

## Android Studio öffnen

```bash
npm run android:open
```

Dann:

- Emulator starten
- oder Handy via USB anschließen
- Run drücken

## APK bauen

In Android Studio:

```txt
Build → Build APK(s)
```

## Ziel

Das Tool soll später:

- VIA/QMK RGB-Modi visualisieren
- Layer simulieren
- Exportfunktionen bieten
- evtl. später WebUSB/WebHID nutzen
- evtl. später echtes Device-Handling bekommen
