#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

using namespace std;

struct Actividad {
	string id_actividad;
	string nombre_actividad;
	int tiempo_ms;
	vector<string> dependencias;
	vector<string> dependientes;
	int grado_entrada;
};

string trim(const string &texto){
	size_t inicio = texto.find_first_not_of(" ");
	if (inicio == string::npos){
		return "";
	}
	size_t fin = texto.find_last_not_of(" ");
	return texto.substr(inicio, fin - inicio + 1);
}

vector<string> dividir(const string &texto, char delimitador){
	vector <string> resultado;
	stringstream stream(texto);
	string pedazo;

	while (getline(stream, pedazo, delimitador)){
		resultado.push_back(pedazo);
	}
	return resultado;
}

bool hayCiclo(unordered_map<string, Actividad> &actividades){
	unordered_map<string, int> grados_temp;
	for (const auto &par : actividades){
		grados_temp[par.first] = par.second.grado_entrada;
	}

	queue<string> listos;
	for (const auto &par : grados_temp){
		if (par.second == 0){
			listos.push(par.first);
		}
	}

	int procesados = 0;
	while (!listos.empty()){
		string actual = listos.front();
		listos.pop();
		procesados++;

		for (const string &dep_id : actividades[actual].dependientes){
			grados_temp[dep_id]--;
			if (grados_temp[dep_id] == 0){
				listos.push(dep_id);
			}
		}
	}

	return procesados != (int)actividades.size();
}

int main(int argc, char *argv[]) {
	srand(time(nullptr));
	unordered_map<string, Actividad> actividades;

	if (argc < 3){
		cerr << "Uso: " << argv[0] << " plan.txt K" << endl;
		return 1;
	}

	int K = stoi(argv[2]);
	if (K <= 0){
		cerr << "K debe ser un número mayor a 0." << endl;
		return 1;
	}

	ifstream archivo(argv[1]);
	if (!archivo.is_open()){
		cerr << "No se pudo abrir el archivo: " << argv[1] << endl;
		return 1;
	}

	string linea;
	while (getline(archivo, linea)){
		linea = trim(linea);
		if (linea.empty()){
			continue;
		}

		vector<string> partes = dividir(linea, ':');
		while (partes.size() < 4){
			partes.push_back("");
		}

		Actividad act;
		act.id_actividad = trim(partes[0]);
		act.nombre_actividad = trim(partes[1]);

		string tiempo_str = trim(partes[2]);
		if (tiempo_str.empty()){
			act.tiempo_ms = 100 + rand() % (5000 - 100 + 1);
		}
		else {
			act.tiempo_ms = stoi(tiempo_str);
		}

		string deps_str = trim(partes[3]);
		if (!deps_str.empty()){
			vector<string> deps = dividir(deps_str, ',');
			for (const string &dep : deps){
				act.dependencias.push_back(trim(dep));
			}
		}

		act.grado_entrada = 0;
		actividades[act.id_actividad] = act;
	}

	archivo.close();

	for (auto &par : actividades){
		Actividad &act = par.second;
		for (const string &dep_id : act.dependencias){
			if (actividades.find(dep_id) == actividades.end()){
				cerr << "Dependencia inexistente: " << dep_id << " (requerida por " << act.id_actividad << ")" << endl;
				return 1;
			}
			actividades[dep_id].dependientes.push_back(act.id_actividad);
			act.grado_entrada++;
		}
	}

	if (hayCiclo(actividades)){
		cerr << "Error: el plan contiene un ciclo, no es un DAG válido." << endl;
		return 1;
	}

	queue<string> cola_listos;
	for (const auto &par : actividades){
		if (par.second.grado_entrada == 0){
			cola_listos.push(par.first);
		}
	}

	int corriendo = 0;
	int terminadas = 0;
	int total = actividades.size();
	unordered_map<pid_t, string> pid_a_id;

	while (terminadas < total){
		while (corriendo < K && !cola_listos.empty()){
			string id_actual = cola_listos.front();
			cola_listos.pop();

			pid_t pid = fork();

			if (pid == 0){
				int tiempo_ms = actividades[id_actual].tiempo_ms;
				struct timespec ts;
				ts.tv_sec = tiempo_ms / 1000;
				ts.tv_nsec = (tiempo_ms % 1000) * 1000000;
				nanosleep(&ts, nullptr);
				cout << "Actividad " << id_actual << " (" << actividades[id_actual].nombre_actividad << ") terminada." << endl;
				exit(0);
			}
			else if (pid > 0){
				pid_a_id[pid] = id_actual;
				corriendo++;
			}
			else {
				cerr << "Error al crear proceso para actividad " << id_actual << endl;
				return 1;
			}
		}

		if (corriendo > 0){
			int status;
			pid_t pid_terminado = waitpid(-1, &status, 0);
			corriendo--;
			terminadas++;

			string id_terminado = pid_a_id[pid_terminado];

			for (const string &dep_id : actividades[id_terminado].dependientes){
				actividades[dep_id].grado_entrada--;
				if (actividades[dep_id].grado_entrada == 0){
					cola_listos.push(dep_id);
				}
			}
		}
	}

	return 0;
}
