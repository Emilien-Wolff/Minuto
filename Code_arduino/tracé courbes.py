import pandas as pd
import matplotlib.pyplot as plt

# Lire le fichier texte
file_path = "démonstration.txt"  # Remplacez par le chemin de votre fichier
data = pd.read_csv(file_path, sep=';', engine='python')

# Supprimer les colonnes inutiles et les lignes vides
data = data.dropna(how="all")

# Extraire les colonnes nécessaires
time = data.iloc[:, 0]  # Temps (s)
irradiance = data.iloc[:, 1]  # Irradiance (W/m²)
delta_t = data.iloc[:, 2]  # DeltaT (K) comme incertitude pour la température
temperature = data.iloc[:, 3]  # Température (°C)

print(irradiance)

# Définir les barres d'incertitude
uncertainty_irradiance = 38  # Exemple : 5% de l'irradiance
uncertainty_temp = 0.01  # Utiliser DeltaT comme incertitude pour la température

# Création du graphique avec deux axes
fig, ax1 = plt.subplots(figsize=(10, 6))

# Tracé de la température avec `plot`
color_temp = 'tab:red'
ax1.set_xlabel('Temps (s)')
ax1.set_ylabel('Température (°C)', color=color_temp)
ax1.plot(time, temperature, 'o-', color=color_temp, label="Température")
ax1.fill_between(time, temperature - uncertainty_temp, temperature + uncertainty_temp, color=color_temp, alpha=0.2, label="Incertitude Température")
ax1.tick_params(axis='y', labelcolor=color_temp)
ax1.grid()

# Ajouter une seconde axe pour l'irradiance avec `plot`
ax2 = ax1.twinx()
color_irradiance = 'tab:blue'
ax2.set_ylabel('Irradiance (W/m²)', color=color_irradiance)
ax2.plot(time, irradiance, 's-', color=color_irradiance, label="Irradiance")
ax2.fill_between(time, irradiance - uncertainty_irradiance, irradiance + uncertainty_irradiance, color=color_irradiance, alpha=0.2, label="Incertitude Irradiance")
ax2.tick_params(axis='y', labelcolor=color_irradiance)

# Ajouter une légende combinée
ax1.legend(loc="upper left")
ax2.legend(loc="upper right")

# Titre et ajustement
plt.title("Température et Irradiance en fonction du Temps")
fig.tight_layout()
plt.show()
