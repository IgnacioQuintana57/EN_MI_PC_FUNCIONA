import requests
import ctypes

URL = "https://api.worldbank.org/v2/en/country/all/indicator/SI.POV.GINI?format=json&date=2011:2020&per_page=32500&page=1&country=%22Argentina%22"

def obtener_gini_argentina():
    respuesta = requests.get(URL)

    if respuesta:
        print(f"Conexión exitosa - Status: {respuesta.status_code}")
    else:
        print(f"Error al conectar - Status: {respuesta.status_code}")
        return []

    datos = respuesta.json()    
    registros = datos[1]        # El primer elemento es metadata, el segundo es la lista de registros
    resultados = []             # Lista para almacenar los resultados (año, valor)

    for registro in registros:
        if registro["country"]["value"] == "Argentina" and registro["value"] is not None:
            anio = int(registro["date"])
            valor = float(registro["value"])
            resultados.append((anio, valor))

    resultados.sort()
    return resultados


if __name__ == "__main__":
    datos = obtener_gini_argentina()

    # Cargar la biblioteca compartida y configurar la función
    lib = ctypes.CDLL('./libgini.so')
    lib.procesar_gini.argtypes = (ctypes.c_double,)
    lib.procesar_gini.restype = ctypes.c_long

    if datos:
        print("\nÍndice GINI de Argentina (2011-2020):")
        print("-" * 40)
        for anio, valor in datos:
            resultado = lib.procesar_gini(valor)
            print(f"Año {anio}: {valor} → Procesado = {resultado}")
    else:
        print("No se pudieron obtener los datos.")