import gym
import ray
from ray.rllib.agents.ppo import PPOTrainer, DEFAULT_CONFIG
from ray.tune.logger import pretty_print

ray.init(num_cpus=6, ignore_reinit_error=True, log_to_driver=False)

config = DEFAULT_CONFIG.copy()
config["framework"] = "torch"
config['num_workers'] = 3
config['num_sgd_iter'] = 30
config['sgd_minibatch_size'] = 128
config['model'] = {'fcnet_hiddens': [100, 100], 
    "fcnet_activation": "relu"}
config['num_cpus_per_worker'] = 2
# config['num_gpus_per_worker'] = 1
config["entropy_coeff"] = 0.1

# import pdb; pdb.set_trace()

agent = PPOTrainer(config, 'CartPole-v1')

for i in range(2):
    result = agent.train()
    print(pretty_print(result))

checkpoint_path = agent.save()
print(checkpoint_path)
agent.cleanup()


agent = PPOTrainer(config, 'CartPole-v1')
agent.restore(checkpoint_path)

env = gym.make('CartPole-v1')
state = env.reset()
env.render()
done = False
cumulative_reward = 0

while not done:
    action = agent.compute_single_action(state)
    state, reward, done, _ = env.step(action)
    env.render()
    cumulative_reward += reward

print(cumulative_reward)