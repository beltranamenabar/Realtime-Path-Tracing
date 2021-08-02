import csv
import matplotlib.pyplot as plt


screenSize = []
dedicated = []
integrated = []

with open('tamano_pantalla.csv') as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    line_count = 0
    for row in csv_reader:
        screenSize.append(float(row[0]))
        dedicated.append(float(row[1]))
        integrated.append(float(row[2]))


#plt.scatter(screenSize, dedicated, label="GPU dedicada")
#plt.scatter(screenSize, integrated, label="GPU integrada")
#plt.title("Tiempo de ejecución del kernel vs tamaño de la imagen")
#plt.ylabel('Tiempo (ms)')
#plt.xlabel("Tamaño de la imagen (pixeles)")
#plt.legend()
#plt.savefig("Tamaño Pantalla.png")

bounces = []
dedicated = []
integrated = []

with open('tamano_pantalla.csv') as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    line_count = 0
    for row in csv_reader:
        bounces.append(float(row[0]))
        dedicated.append(float(row[1]))
        integrated.append(float(row[2]))

plt.clf()

plt.scatter(bounces, dedicated, label="GPU dedicada")
plt.scatter(bounces, integrated, label="GPU integrada")
plt.title("Tiempo de ejecución del kernel vs cantidad de rebotes")
plt.ylabel('Tiempo (ms)')
plt.xlabel("Cantidad de rebotes")
plt.legend()
plt.savefig("rebotes.png")