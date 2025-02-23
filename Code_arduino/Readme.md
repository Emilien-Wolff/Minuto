# Code - Capteur d'Irradiance Solaire  

Ce dossier contient :  
- **`etalonnage.py`** : Script Python pour tracer les courbes d'étalonnage du capteur en relevant les couples **tension - température**.  
- **`capteur_irradiance.ino`** : Code Arduino calculant et affichant l'irradiance solaire sur un écran **OLED 128x64** sur une durée **Δt** modifiable dans le code.  

## Utilisation  

- **Étalonnage (Python)** :  
  ```sh
  pip install matplotlib numpy  
  python etalonnage.py  
  ```  
- **Mesure (Arduino)** :  
  1. Ouvrir `capteur_irradiance.ino` dans l'IDE Arduino.  
  2. Modifier **Δt** si nécessaire.  
  3. Téléverser le code sur un **Arduino R4 Wifi**.  

📌 *Vérifiez les connexions du capteur et de l'afficheur OLED avant exécution.*

