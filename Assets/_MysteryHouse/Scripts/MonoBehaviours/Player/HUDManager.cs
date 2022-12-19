using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class HUDManager : MonoBehaviour
{
    public Action<int> onInitializeHealthBar;
    public Action<int> onSetHealthBar;

    // Start is called before the first frame update
    void Start()
    {
       
    }

    public void InitializeHealthBar(int healthParam)
    {
        onInitializeHealthBar.Invoke(healthParam);
    }

    public void SetHealth(int healthParam)
    {
        onSetHealthBar.Invoke(healthParam);
    }
}
