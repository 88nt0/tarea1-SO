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
#include <csignal>
#include <signal.h>

using namespace std;

volatile sig_atomic_t interrumpido = 0;

void manejarSigint(int señal){
	interrumpido = 1;
}

struct Actividad {
	string id_actividad;
	string nombre_actividad;
	int tiempo_ms;
	vector<string> dependencias;
	vector<string> dependientes;
	int grado_entrada;
	bool abortada = false;
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

	struct sigaction sa;
	sa.sa_handler = manejarSigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, nullptr);

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
		cerr << "Error: el plan contiene un ciclo, no es un DAG valido." << endl;
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

	while (terminadas < total && !interrumpido){
		while (corriendo < K && !cola_listos.empty() && !interrumpido){
			string id_actual = cola_listos.front();
			cola_listos.pop();

			int fd[2];
			if (pipe(fd) == -1){
				cerr << "Error al crear pipe para actividad " << id_actual << endl;
				return 1;
			}

			string mensaje = "Insumo de: ";
			for (const string &dep : actividades[id_actual].dependencias){
				mensaje += actividades[dep].nombre_actividad + " ";
			}

			pid_t pid = fork();

			if (pid == 0){
				close(fd[1]);

				char buffer[256];
				int bytes_leidos = read(fd[0], buffer, sizeof(buffer) - 1);
				if (bytes_leidos > 0){
					buffer[bytes_leidos] = '\0';
					cout << "Actividad " << id_actual << " recibio: " << buffer << endl;
				}
				close(fd[0]);

				if (rand() % 100 < 5){
					cerr << "Actividad " << id_actual << " fallo." << endl;
					exit(1);
				}

				int tiempo_ms = actividades[id_actual].tiempo_ms;
				struct timespec ts;
				ts.tv_sec = tiempo_ms / 1000;
				ts.tv_nsec = (tiempo_ms % 1000) * 1000000;
				nanosleep(&ts, nullptr);
				cout << "Actividad " << id_actual << " (" << actividades[id_actual].nombre_actividad << ") terminada." << endl;
				exit(0);
			}
			else if (pid > 0){
				close(fd[0]);
				write(fd[1], mensaje.c_str(), mensaje.size());
				close(fd[1]);

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
			if (pid_terminado > 0){
				corriendo--;
				terminadas++;

				string id_terminado = pid_a_id[pid_terminado];
				bool fallo = WIFEXITED(status) && WEXITSTATUS(status) != 0;

				if (fallo){
					cerr << "Actividad " << id_terminado << " fallo, abortando dependientes." << endl;
					queue<string> por_abortar;
					for (const string &dep_id : actividades[id_terminado].dependientes){
						por_abortar.push(dep_id);
					}
					while (!por_abortar.empty()){
						string actual_abortar = por_abortar.front();
						por_abortar.pop();
						if (actividades[actual_abortar].abortada){
							continue;
						}
						actividades[actual_abortar].abortada = true;
						terminadas++;
						cerr << "Actividad " << actual_abortar << " abortada (dependia de " << id_terminado << ")." << endl;
						for (const string &siguiente : actividades[actual_abortar].dependientes){
							por_abortar.push(siguiente);
						}
					}
				}
				else {
					for (const string &dep_id : actividades[id_terminado].dependientes){
						if (actividades[dep_id].abortada){
							continue;
						}
						actividades[dep_id].grado_entrada--;
						if (actividades[dep_id].grado_entrada == 0){
							cola_listos.push(dep_id);
						}
					}
				}
			}
		}
	}

	if (interrumpido){
		cerr << endl << "Senal SIGINT recibida, abortando actividades en curso..." << endl;

		for (const auto &par : pid_a_id){
			kill(par.first, SIGTERM);
		}

		int procesos_restantes = corriendo;
		for (int i = 0; i < procesos_restantes; i++){
			int status;
			waitpid(-1, &status, 0);
		}

		cout << "Resumen: " << terminadas << " actividades finalizadas de " << total << " totales." << endl;
		return 1;
	}

	cout << "Todas las actividades finalizaron. Total: " << terminadas << "/" << total << endl;
	return 0;
}
