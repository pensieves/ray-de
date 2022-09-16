from gym import Env
from ns3gym import ns3env

from gym_utils import change_space_dtype, to_space_dtype


class RayRLlibNS3Env(Env):

    def __init__(self, env_config={}):
        r"""ray[rllib] currently don't support uints as observation types.
        Need to map them to int types.
        env_config should be the dictionary expected by the underlying Ns3Env
        """

        super(RayRLlibNS3Env, self).__init__()
        
        self.env = ns3env.Ns3Env(**env_config)
        
        self.dtype_map = dict(uint="int", uint32="int32", uint64="int64")
        
        self.observation_space = change_space_dtype(
            self.env.observation_space,
            dtype_map=self.dtype_map,
        )
        
        self.action_space = change_space_dtype(
            self.env.action_space,
            dtype_map=self.dtype_map,
        )

        self.state = to_space_dtype(self.env.state, self.observation_space)


    def reset(self):
        self.state = to_space_dtype(self.env.reset(), self.observation_space)
        return self.state


    def step(self, action):
        r"""Convert observation to ray[rllib]/torch compatible dtypes.
        ray[rllib] expects info to be a dict, hence convert it into a dict.
        """
        obs, reward, done, info = self.env.step(action)
        self.state = to_space_dtype(obs, self.observation_space)
        info = {"info": info}
        return self.state, reward, done, info
