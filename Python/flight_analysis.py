#Imports
import matplotlib.pyplot as plt
from pathlib import Path
import os

WINDOW_SIZE = 10  # Size of the moving average window for smoothing altitude data
FIGURE_SIZE = (15, 11)  # Size of the figure for the report

FLIGHT_LOG_DIR = "FlightLogs"
IMAGE_DIR = "Images"

def choose_flight():
    """Displays available flight logs and returns the selected flight path and flight name"""

    flight_logs = []

    #Get all CSV files in the FlightLogs directory
    for filename in os.listdir(FLIGHT_LOG_DIR):
        if filename.endswith(".csv"):
            flight_logs.append(os.path.join(FLIGHT_LOG_DIR, filename))

    flight_logs.sort()

    if len(flight_logs) == 0:
        print("No flight logs found in the FlightLogs directory.")
        raise SystemExit

    #User chooses which flight log to view data for
    print("Available Flight Logs:\n")

    for i, file in enumerate(flight_logs):
        print(f"{i + 1}. {Path(file).name}")

    while True:
        try:
            choice = int(input("\nSelect a flight: "))

            if 1 <= choice <= len(flight_logs):
                break

            print("Invalid Flight Log number. Please try again.")

        except ValueError:
            print("Invalid input. Please enter a number.")

    file_path = flight_logs[choice - 1]
    flight_name = Path(file_path).stem  # Get the flight name without extension

    return file_path, flight_name

def load_flight_data(file_path):
    """Loads flight telemetry from CSV file"""

    times = []
    altitudes = []
    accelerations = []
    flight_states = []

    with open(file_path, "r") as file:
        file.readline()  # Skip the header line

        for line in file:
            values = line.split(',')

            times.append(float(values[0]) / 1000.0) 
            altitudes.append(float(values[3]))
            accelerations.append(float(values[4]))
            flight_states.append(int(values[11]))

    max_altitude = max(altitudes)
    max_accel = max(accelerations)

    return times, altitudes, accelerations, flight_states, max_altitude, max_accel

def find_flight_events(times, flight_states):
    """Uses flight state data to identify the launch, apogee, and landing times of the flight"""

    #First time state appears, initialized to None
    launch_time = None
    apogee_time = None
    landing_time = None

    #Identify the first occurrence of each flight state
    for i in range(len(flight_states)):
        if flight_states[i] == 1 and launch_time is None:
            launch_time = times[i]
        elif flight_states[i] == 2 and apogee_time is None:
            apogee_time = times[i]
        elif flight_states[i] == 3 and landing_time is None:
            landing_time = times[i]

    return launch_time, apogee_time, landing_time

def calculate_velocity(times, altitudes):
    """Calculates the velocity of the flight based on altitude data using a moving average for smoothing"""

    smoothed_altitudes = [] 

    # Smoothing altitude data using a moving average
    for i in range(len(altitudes)):
        start = max(0, i - WINDOW_SIZE + 1)
        window = altitudes[start:i + 1]
        average = sum(window) / len(window)
        smoothed_altitudes.append(average)

    velocities = [0]

    #Calculate velocity based on smoothed altitude data
    for i in range(1, len(times)):
        delta_altitude = smoothed_altitudes[i] - smoothed_altitudes[i - 1]
        delta_time = times[i] - times[i - 1]

        if delta_time > 0:
            velocity = delta_altitude / delta_time
        else:
            velocity = 0  # Avoid division by zero

        velocities.append(velocity)

    return velocities

def create_report(
    times,
    altitudes,
    accelerations,
    velocities,
    launch_time,
    apogee_time,
    landing_time,
    max_altitude,
    max_accel,
    flight_name,
):
    """Creates a report with subplots for altitude, acceleration, and velocity, along with a statistics box summarizing key flight metrics. Saves the report as an image and displays it."""

    os.makedirs(IMAGE_DIR, exist_ok=True)  # Ensure the Images directory exists

    #Statistics Box
    summary_text = (
        f"Max Altitude: {max_altitude:.2f} m\n"
        f"Max Acceleration Magnitude: {max_accel:.2f} m/s²\n"
        f"Launch: {launch_time:.2f} s\n"
        f"Apogee: {apogee_time:.2f} s\n"
        f"Landing: {landing_time:.2f} s\n"
        f"Time to Apogee: {apogee_time - launch_time:.2f} s\n"
        f"Flight Duration: {landing_time - launch_time:.2f} s"
    )


    plt.figure(figsize=FIGURE_SIZE)

    plt.subplot(3, 1, 1)

    #Altitude Plot
    plt.plot(times, altitudes, label="Altitude")
    plt.title("Flight Altitude Analysis")
    plt.xlabel("Time (s)")
    plt.ylabel("Altitude (m)")
    plt.grid(True)

    plt.axvline(launch_time, color="green", linestyle="--", label="Launch")
    plt.axvline(landing_time, color="red", linestyle="--", label="Landing")
    plt.axvline(apogee_time, color="orange", linestyle="--", label="Apogee")

    plt.legend()

    plt.subplot(3, 1, 2)

    #Acceleration Plot
    plt.plot(times, accelerations, label="Acceleration Magnitude")
    plt.title("Flight Acceleration Analysis")
    plt.xlabel("Time (s)")
    plt.ylabel("Acceleration Magnitude (m/s²)")
    plt.grid(True)

    plt.axvline(launch_time, color="green", linestyle="--", label="Launch")
    plt.axvline(landing_time, color="red", linestyle="--", label="Landing")
    plt.axvline(apogee_time, color="orange", linestyle="--", label="Apogee")

    plt.legend()

    plt.subplot(3, 1, 3)

    #Velocity Plot
    plt.plot(times, velocities, label="Velocity")
    plt.title("Flight Velocity Analysis")
    plt.xlabel("Time (s)")
    plt.ylabel("Velocity (m/s)")
    plt.grid(True)

    plt.axvline(launch_time, color="green", linestyle="--", label="Launch")
    plt.axvline(landing_time, color="red", linestyle="--", label="Landing")
    plt.axvline(apogee_time, color="orange", linestyle="--", label="Apogee")

    plt.legend()

    plt.suptitle(f"{flight_name} Flight Analysis")

    plt.figtext(
        .76,
        .88,
        summary_text,
        ha="left",
        va="top",
    )

    plt.tight_layout(rect=[0, 0, 0.75, 0.95])  # Adjusted layout to make room for the statistics box
    plt.savefig(f"{IMAGE_DIR}/{flight_name}_report.png")
    plt.show()

#Main Program
def main():
    file_path, flight_name = choose_flight()

    times, altitudes, accelerations, flight_states, max_altitude, max_accel = load_flight_data(file_path)

    launch_time, apogee_time, landing_time = find_flight_events(times, flight_states)

    velocities = calculate_velocity(times, altitudes)

    create_report(times, altitudes, accelerations, velocities, launch_time, apogee_time, landing_time, max_altitude, max_accel, flight_name)

if __name__ == "__main__":
    main()