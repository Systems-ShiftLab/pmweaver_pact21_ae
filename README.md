Artifact repository for PMWeaver. 

This readme contains basic commands to reproduce results of the experiments. For more details, check the appendix of the paper.

## PMWeaver Docker image

1. Build the docker image
```shell
docker build -t pmweaver .
```

2. Start the docker container
```shell
docker run -i -t -p 8888:8888 pmweaver /bin/bash
```

## Running experiments
To run partX:
```shell
scripts/run_partX.py
```

To check progress:
```shell
scripts/helper_scripts/check_progress.sh
```

To kill all stray processes between runs (doesn't delete data):
```shell
scripts/helper_scripts/kill_stray.sh
```

To plot results of partX:
```shell
scripts/plot_scripts/plot_partX.py
```

To start an HTTP server to view results (Located at: `scripts/plots/ae/`):
```shell
scripts/helper_scripts/start_server.sh
```
