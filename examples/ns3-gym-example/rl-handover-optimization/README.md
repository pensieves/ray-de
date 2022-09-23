Execute `python3 env-test.py 1` from the current directory to test the env in ns3 debug mode. The execution should end with "DONE" displayed at the end. The test currently doesn't work without the debug mode i.e. `python3 env-test.py 0`, but can be used with the understanding that the debug mode prints debugging information while execution.

To train / infer the rl policy based handover optimization, edit the parameters of the `run_config_dqn.yaml` or `run_config_ppo.yaml` file and then execute the `rllib-agent.py` file as:

```
python3 rllib-agent.py --config run_config_dqn.yaml
```