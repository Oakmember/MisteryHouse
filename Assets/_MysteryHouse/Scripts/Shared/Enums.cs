
using System;

namespace Shared
{
    public enum HandType { None = 0, Left = 1, Right = 2, Both = 3 }

    public enum HandStateType { None = 0, Grab = 1, Drop = 2 }

    public enum PropStateType { None = 0, Grabbed = 1, Dropped = 2 }

    public enum QuestType { None = 0, HingedDoors, SlidingDoors, Drawer, }


    public enum HVRAxis
    {
        X, Y, Z,
        NegX, NegY, NegZ
    }

    [Serializable]
    public struct HVRButtonState
    {
        public bool Active;
        public bool JustActivated;
        public bool JustDeactivated;
        public float Value;
    }
}