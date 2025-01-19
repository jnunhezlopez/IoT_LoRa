import tkinter as tk
from tkinter import ttk, messagebox
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import paho.mqtt.client as mqtt
import json
import sqlite3

# Variables globales para almacenar los datos
current_temperature = 0.0
current_humidity = 0.0
min_temp_alert = 0.0
max_temp_alert = 50.0
min_hum_alert = 0.0
max_hum_alert = 100.0

# Configuración de SQLite
def init_db():
    conn = sqlite3.connect("iot_data.db")
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature REAL,
            humidity REAL
        )
    ''')
    conn.commit()
    conn.close()

# Función para guardar datos en SQLite
def save_to_db(temperature, humidity):
    conn = sqlite3.connect("iot_data.db")
    cursor = conn.cursor()
    cursor.execute("INSERT INTO sensor_data (temperature, humidity) VALUES (?, ?)", (temperature, humidity))
    conn.commit()
    conn.close()

# Función para manejar mensajes MQTT
def on_message(client, userdata, msg):
    global current_temperature, current_humidity
    try:
        payload = json.loads(msg.payload.decode('utf-8'))
        current_temperature = payload.get('temperature', 0.0)
        current_humidity = payload.get('humidity', 0.0)
        update_gui()
        check_alerts()
        save_to_db(current_temperature, current_humidity)  # Guardar en SQLite
    except json.JSONDecodeError:
        print("Error decodificando JSON")

# Función para verificar alertas
def check_alerts():
    if current_temperature < min_temp_alert or current_temperature > max_temp_alert:
        show_alert("Temperatura fuera de rango", f"{current_temperature:.2f} °C")
    if current_humidity < min_hum_alert or current_humidity > max_hum_alert:
        show_alert("Humedad fuera de rango", f"{current_humidity:.2f} %")

# Función para mostrar alertas
def show_alert(title, message):
    messagebox.showwarning(title, message)

# Función para actualizar la GUI
def update_gui():
    temperature_label.config(text=f"Temperatura: {current_temperature:.2f} °C")
    humidity_label.config(text=f"Humedad: {current_humidity:.2f} %")
    update_plot()

# Configuración inicial de la gráfica
fig = Figure(figsize=(6.8, 3.4), dpi=100)  # Ajuste para que la gráfica encaje bien en la resolución
ax = fig.add_subplot(111)
temperature_data = []
humidity_data = []
time_data = []

# Función para actualizar la gráfica
def update_plot():
    
    time_data.append(len(time_data))
    temperature_data.append(current_temperature)
    humidity_data.append(current_humidity)

    # if len(time_data) > 50:  # Limitar a 50 puntos para claridad
    #     time_data.pop(0)
    #     temperature_data.pop(0)
    #     humidity_data.pop(0)

    ax.clear()
    ax.plot(time_data, temperature_data, label='Temperatura (°C)', color='red')
    ax.plot(time_data, humidity_data, label='Humedad (%)', color='blue')
    ax.set_title('Temperatura y Humedad en Tiempo Real')
    ax.set_xlabel('Tiempo (s)')
    ax.set_ylabel('Valor')
    ax.legend()
    ax.grid()
    canvas.draw()

# Función para guardar los límites configurados
def save_limits():
    global min_temp_alert, max_temp_alert, min_hum_alert, max_hum_alert
    try:
        min_temp_alert = float(min_temp_entry.get())
        max_temp_alert = float(max_temp_entry.get())
        min_hum_alert = float(min_hum_entry.get())
        max_hum_alert = float(max_hum_entry.get())
        messagebox.showinfo("Configuración", "Límites actualizados correctamente")
    except ValueError:
        messagebox.showerror("Error", "Por favor, introduzca valores numéricos válidos")

# Función para configurar el intervalo de envío
def set_interval():
    try:
        interval = int(interval_entry.get())
        command = json.dumps({"command": "set_interval", "value": interval})
        client.publish("iot/sensor/commands", command)
        messagebox.showinfo("Configuración", f"Intervalo configurado a {interval} ms")
    except ValueError:
        messagebox.showerror("Error", "Por favor, introduzca un valor numérico válido para el intervalo")

# Configuración de la ventana principal
root = tk.Tk()
root.title("Monitor IoT de Temperatura y Humedad")
root.geometry("800x480")  # Resolución ajustada para pantalla táctil de 7''

# Marco principal con barra de desplazamiento
main_frame = ttk.Frame(root)
main_frame.pack(fill=tk.BOTH, expand=True)

canvas_scroll = tk.Canvas(main_frame)
scrollbar = ttk.Scrollbar(main_frame, orient=tk.VERTICAL, command=canvas_scroll.yview)
scrollable_frame = ttk.Frame(canvas_scroll)

scrollable_frame.bind(
    "<Configure>",
    lambda e: canvas_scroll.configure(scrollregion=canvas_scroll.bbox("all"))
)

canvas_scroll.create_window((0, 0), window=scrollable_frame, anchor="nw")
canvas_scroll.configure(yscrollcommand=scrollbar.set)

canvas_scroll.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

# Configure grid layout for scrollable_frame
scrollable_frame.columnconfigure(0, weight=1)  # Left column for controls
scrollable_frame.columnconfigure(1, weight=2)  # Right column for FigureCanvas

# Left side (controls)
controls_frame = ttk.Frame(scrollable_frame, padding="10")
controls_frame.grid(row=0, column=0, sticky=(tk.N, tk.S, tk.W, tk.E))

# Etiquetas para mostrar datos
temperature_label = ttk.Label(controls_frame, text="Temperatura: 0.00 °C", font=("Arial", 10))
temperature_label.grid(row=0, column=0, pady=5, sticky=tk.W)

humidity_label = ttk.Label(controls_frame, text="Humedad: 0.00 %", font=("Arial", 10))
humidity_label.grid(row=1, column=0, pady=5, sticky=tk.W)

# Controles para configurar límites
limits_frame = ttk.LabelFrame(controls_frame, text="Configuración de Límites", padding="10")
limits_frame.grid(row=2, column=0, pady=10, sticky=(tk.W, tk.E))

min_temp_label = ttk.Label(limits_frame, text="Temperatura mínima:")
min_temp_label.grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
min_temp_entry = ttk.Entry(limits_frame, width=10)
min_temp_entry.grid(row=0, column=1, padx=5, pady=5)
min_temp_entry.insert(0, str(min_temp_alert))

max_temp_label = ttk.Label(limits_frame, text="Temperatura máxima:")
max_temp_label.grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
max_temp_entry = ttk.Entry(limits_frame, width=10)
max_temp_entry.grid(row=1, column=1, padx=5, pady=5)
max_temp_entry.insert(0, str(max_temp_alert))

min_hum_label = ttk.Label(limits_frame, text="Humedad mínima:")
min_hum_label.grid(row=2, column=0, padx=5, pady=5, sticky=tk.W)
min_hum_entry = ttk.Entry(limits_frame, width=10)
min_hum_entry.grid(row=2, column=1, padx=5, pady=5)
min_hum_entry.insert(0, str(min_hum_alert))

max_hum_label = ttk.Label(limits_frame, text="Humedad máxima:")
max_hum_label.grid(row=3, column=0, padx=5, pady=5, sticky=tk.W)
max_hum_entry = ttk.Entry(limits_frame, width=10)
max_hum_entry.grid(row=3, column=1, padx=5, pady=5)
max_hum_entry.insert(0, str(max_hum_alert))

save_button = ttk.Button(limits_frame, text="Guardar", command=save_limits)
save_button.grid(row=4, column=0, columnspan=2, pady=10)

# Controles para configurar el intervalo de envío
interval_frame = ttk.LabelFrame(controls_frame, text="Configuración de Intervalo", padding="10")
interval_frame.grid(row=3, column=0, pady=10, sticky=(tk.W, tk.E))

interval_label = ttk.Label(interval_frame, text="Intervalo de envío (ms):")
interval_label.grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
interval_entry = ttk.Entry(interval_frame, width=10)
interval_entry.grid(row=0, column=1, padx=5, pady=5)
interval_entry.insert(0, "5000")  # Valor predeterminado: 5000 ms

set_interval_button = ttk.Button(interval_frame, text="Configurar", command=set_interval)
set_interval_button.grid(row=1, column=0, columnspan=2, pady=10)

# Right side (FigureCanvas)
canvas = FigureCanvasTkAgg(fig, master=scrollable_frame)
canvas_widget = canvas.get_tk_widget()
canvas_widget.configure(width=400, height=300)
canvas_widget.grid(row=0, column=1, padx=10, pady=10, sticky=(tk.N, tk.S, tk.E, tk.W))

# Make canvas_widget responsive
root.rowconfigure(0, weight=1)
root.columnconfigure(0, weight=1)
scrollable_frame.rowconfigure(0, weight=1)
scrollable_frame.columnconfigure(1, weight=1)

# Configuración del cliente MQTT
client = mqtt.Client()
client.on_message = on_message

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conexión exitosa al broker MQTT")
        client.subscribe("iot/lora")
    else:
        print(f"Conexión fallida, código de error {rc}")

client.on_connect = on_connect
client.connect("192.168.0.28", 1883, 60)  # Ajusta según la configuración de tu broker MQTT
client.loop_start()
init_db()
root.mainloop()
