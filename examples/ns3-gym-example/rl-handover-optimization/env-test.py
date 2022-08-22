from sys import argv
from ns3gym import ns3env

# argv[1] = 0 -> without debugging; = 1 -> with debugging
debug = bool(int(argv[1]))
env = ns3env.Ns3Env(debug=debug)
obs_space = env.observation_space
act_space = env.action_space
print(obs_space)
print(act_space)

s = env.reset()
print(s)

done = False
while not done:
    action = env.action_space.sample()
    obs, reward, done, info = env.step(action)
    print(action, obs, reward, done, info)
print("DONE.")
