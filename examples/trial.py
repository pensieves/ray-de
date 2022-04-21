import gym
import panda_gym
from panda_gym.envs import PandaStackEnv
import ray
from ray.rllib.agents.sac import SACTrainer, DEFAULT_CONFIG
from ray.tune.logger import pretty_print


NUM_WORKERS = 2
NUM_TRAIN_ITER = 2
REPORT_FREQ = 1
ENV_NAME = "PandaStack-v2"
NUM_CPUS = 3
CPUS_PER_WORKER = 1
SAVE_DIR = "ray_outputs"

ray.init(num_cpus=NUM_CPUS, ignore_reinit_error=True, log_to_driver=False)

# DDPG config
config = DEFAULT_CONFIG.copy()
config["framework"] = "torch"
config["num_workers"] = NUM_WORKERS
config["num_gpus"] = 0
config["num_gpus_per_worker"] = 0
config["num_cpus_per_worker"] = CPUS_PER_WORKER
config["Q_model"]["fcnet_hiddens"] = [256, 256]
config["Q_model"]["fcnet_activation"] = "relu"
config["policy_model"]["fcnet_hiddens"] = [256, 256]
config["policy_model"]["fcnet_activation"] = "relu"
config["prioritized_replay"] = True
config["train_batch_size"] = 256
config["learning_starts"] = 1000
config["min_time_s_per_reporting"] = REPORT_FREQ


agent = SACTrainer(env=PandaStackEnv, config=config)

for i in range(NUM_TRAIN_ITER):
    result = agent.train()
    print(pretty_print(result))

checkpoint_path = agent.save(SAVE_DIR)
print(checkpoint_path)
agent.cleanup()

env = gym.make(ENV_NAME, render=True)
agent = SACTrainer(env=PandaStackEnv, config=config)
agent.restore(checkpoint_path)

state = env.reset()
env.render()
done = False
cumulative_reward = 0

while not done:
    action = agent.compute_single_action(state)
    state, reward, done, info = env.step(action)
    env.render()
    cumulative_reward += reward

print(cumulative_reward)
env.close()