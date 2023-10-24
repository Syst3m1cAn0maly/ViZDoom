package main

import (
	"log"
	"net"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

var cacheDir = "--cache-dir='/ViZDoom'"

func hash(input string) int32 {
	var hash int32
	hash = 5381
	for _, char := range input {
		hash = ((hash << 5) + hash + int32(char))
	}
	if hash < 0 {
		hash = 0 - hash
	}
	return hash
}

func outputCmd(argv []string) string {
	cmd := exec.Command(argv[0], argv[1:]...)
	cmd.Stderr = os.Stderr
	output, err := cmd.Output()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", argv)
	}
	return string(output)
}

func startCmd(cmdstring string) {
	parts := strings.Split(cmdstring, " ")
	cmd := exec.Command(parts[0], parts[1:]...)
	if _, varExists := os.LookupEnv("DEBUG"); varExists {
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
	}
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func startCmdArray(cmdstring ...string) {
	cmd := exec.Command(cmdstring[0], cmdstring[1:]...)
	if _, varExists := os.LookupEnv("DEBUG"); varExists {
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
	}
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func getEntities() []string {
	var args []string
	var outputstr string
	if namespace, exists := os.LookupEnv("NAMESPACE"); exists {
		args = dplArgsForNamespace(namespace)
	} else {
		args = []string{"kubectl", "get", "deployments", "-A", cacheDir, "-o", "go-template", "--template={{range .items}}{{.metadata.namespace}}/{{.metadata.name}} {{end}}"}
	}
	output := outputCmd(args)
	outputstr = strings.TrimSpace(output)
	dpls := strings.Split(outputstr, " ")
	return dpls

}

func dplArgsForNamespace(namespace string) []string {
	var args = []string{"kubectl", "get", "deployments", "--namespace", namespace, cacheDir, "-o", "go-template", "--template={{range .items}}{{.metadata.namespace}}/{{.metadata.name}} {{end}}"}
	return args
}

func deleteEntity(entity string) {
	log.Printf("Deployment to kill: %v", entity)
	parts := strings.Split(entity, "/")
	cmd := exec.Command("/usr/bin/kubectl", "delete", "deployment", "--now", "-n", parts[0], parts[1], cacheDir)
	go cmd.Run()
}

func socketLoop(listener net.Listener) {
	strbytes := ""
	var entities []string
	for {
		// lenghty operations before the connection to avoid stutter
		if strings.HasPrefix(strbytes, "kill ") {
			parts := strings.Split(strbytes, " ")
			killhash, err := strconv.ParseInt(parts[1], 10, 32)
			if err != nil {
				log.Fatal("Could not parse kill hash")
			}
			for _, entity := range entities {
				if hash(entity) == int32(killhash) {
					deleteEntity(entity)
					break
				}
			}
		}
		entities = getEntities()
		conn, err := listener.Accept()
		if err != nil {
			panic(err)
		}
		stop := false
		for !stop {
			bytes := make([]byte, 40960)
			n, err := conn.Read(bytes)
			if err != nil {
				stop = true
			}
			bytes = bytes[0:n]
			strbytes = strings.TrimSpace(string(bytes))
			if strbytes == "list" {
				for _, entity := range entities {
					padding := strings.Repeat("\n", 255-len(entity))
					_, err = conn.Write([]byte(entity + padding))
					if err != nil {
						log.Fatal("Could not write to socket file")
					}
				}
				conn.Close()
				stop = true
			} else if strings.HasPrefix(strbytes, "kill ") {
				conn.Close()
				stop = true
			}
		}
	}
}

func main() {

	os.Remove("/app/socket/dockerdoom.socket")

	listener, err := net.Listen("unix", "/ViZDoom/socket/dockerdoom.socket")
	if err != nil {
		log.Fatalf("Could not create socket file")
	}

	startCmd("/usr/bin/Xvfb :99 -ac -screen 0 320x240x24")
	time.Sleep(time.Duration(2) * time.Second)
	startCmd("x11vnc -geometry 320x240 -forever -rfbport 9090 -display :99")

	log.Print("Trying to start backend ...")
	startCmdArray("/bin/bash", "-c", "until /usr/bin/env DISPLAY=:99 python3 -m sf_examples.vizdoom_examples.enjoy_vizdoom --env=doom_battle --algo=APPO --experiment=sample-factory-2-doom-battle --train_dir ./train_dir --device cpu ; do sleep 1 ; echo 'Redémarrage du process' 1>&2 ; done")
	log.Print("Backend started ...")
	socketLoop(listener)
}
