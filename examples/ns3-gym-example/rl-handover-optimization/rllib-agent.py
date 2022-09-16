from tqdm import tqdm
import gym
from RayRLlibNS3Env import RayRLlibNS3Env

import torch
import ray
from ray.tune.logger import pretty_print

import yaml
import numpy as np
from pathlib import Path
from copy import deepcopy
from deepmerge import Merger
from importlib import import_module

def load_last_checkpoint(agent, checkpoint_dir="ray_outputs"):
    checkpoint_dir = Path(checkpoint_dir)
    if checkpoint_dir.exists():
        print(f"Checkpoint dir at {checkpoint_dir} exists. Loading latest checkpoint.")
        checkpoint_iters = [d.name.split("_")[-1] for d in checkpoint_dir.glob("checkpoint*")]
        checkpoint_iter_argmax = np.argmax([int(i) for i in checkpoint_iters])
        checkpoint_path = str(checkpoint_dir/f"checkpoint_{checkpoint_iters[checkpoint_iter_argmax]}"/f"checkpoint-{int(checkpoint_iters[checkpoint_iter_argmax])}")
        agent.restore(checkpoint_path)
    print(f"No checkpoint dir at {checkpoint_dir}. Agent loaded with fresh weights.")

with open("run_config.yaml", "r") as f:
    config = yaml.safe_load(f)

ray.init(**config["ray_config"])

merger = Merger(
    [(dict, ["merge"])], # list of tuples of type and their merge strategies
    ["override"], # fallback merge strategies for all other types
    ["override"], # merge strategies in case of type conflicts
)

agent_name = config["agent_config"].pop("agent")
agent_module, agent_trainer = agent_name.split(".")
agent_module = import_module(f"ray.rllib.agents.{agent_module}")

agent_trainer = agent_module.__dict__[agent_trainer]
trainer_config = deepcopy(agent_module.DEFAULT_CONFIG)
trainer_config = merger.merge(trainer_config, config["agent_config"])
trainer_config["env_config"] = deepcopy(config["env_config"])

if config["other_config"]["num_train_iter"]:
    agent = agent_trainer(env=RayRLlibNS3Env, config=trainer_config)
    load_last_checkpoint(agent, checkpoint_dir=config["other_config"]["save_dir"])

    for i in tqdm(range(config["other_config"]["num_train_iter"])):
        result = agent.train()
        if (i+1)%config["other_config"]["save_freq"] == 0:
            print(pretty_print(result))

            checkpoint_path = agent.save(config["other_config"]["save_dir"])
            print(checkpoint_path)

    agent.cleanup()

print("Evaluating on an environment run")
env = RayRLlibNS3Env(trainer_config["env_config"])
agent = agent_trainer(env=RayRLlibNS3Env, config=trainer_config)
load_last_checkpoint(agent, checkpoint_dir=config["other_config"]["save_dir"])

state = env.reset()
print(f"Initial state: {state}")
# env.render()
done = False
cumulative_reward = 0

while not done:
    action = agent.compute_single_action(state)
    state, reward, done, info = env.step(action)
    # env.render()
    cumulative_reward += reward
    print("Step result:", action, state, reward, done, info, cumulative_reward)

print("Cumulative reward achieved:", cumulative_reward)
env.close()