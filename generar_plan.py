import random

NUM_ACTIVIDADES = 10000

with open("tests/plan_estres.txt", "w") as f:
	for i in range(1, NUM_ACTIVIDADES + 1):
		nombre = f"actividad_{i}"
		tiempo = random.randint(10, 100)
		
		deps = []
		if i > 1:
			num_deps = random.randint(0, min(3, i - 1))
			deps = random.sample(range(1, i), num_deps)
		
		deps_str = ", ".join(str(d) for d in deps)
		f.write(f"{i} : {nombre} : {tiempo} : {deps_str}\n")

print(f"Generado tests/plan_estres.txt con {NUM_ACTIVIDADES} actividades.")
