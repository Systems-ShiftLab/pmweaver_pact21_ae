


If you find PMWeaver useful in your research, please cite:

> Suyash Mahar, Sihang Liu, Korakit Seemakhupt, Vinson Young, Samira Khan  
> [Write Prediction for Persistent Memory Systems](https://suyashmahar.com/resources/papers/pmweaver_pact21.pdf)  
> 30th International Conference on Parallel Architectures and Compilation Techniques (PACT), 2021


<details><summary><i>BibTex</i></summary>
<p>

```
@inproceedings{mahar2021write,
  title={Write Prediction for Persistent Memory Systems},
  author={Mahar, Suyash and Liu, Sihang and Seemakhupt, Korakit and Young, Vinson and Khan, Samira},
  booktitle={2021 30th International Conference on Parallel Architectures and Compilation Techniques (PACT)},
  pages={242--257},
  year={2021},
  organization={IEEE}
}
```

</p>
</details>

# Artifact repository for PMWeaver. 
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
