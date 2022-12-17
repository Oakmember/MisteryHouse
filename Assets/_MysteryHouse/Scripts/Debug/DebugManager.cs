using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;
using TMPro;

public class DebugManager : MonoBehaviour
{
    public static DebugManager instance;

    [SerializeField]
    private TextMeshProUGUI debugText1 = null;

    [SerializeField]
    private TextMeshProUGUI debugText2 = null;

    public static DebugManager Instance
    {
        get
        {
            return instance;
        }
    }

    private void Awake()
    {
        if (instance != null && instance != this)
        {
            Destroy(this.gameObject);
        }
        instance = this;

    }

    void Start()
    {
    }

    public void SetDebugText1(string debugTextParam)
    {
        debugText1.text = debugTextParam;
    }

    public void SetDebugText2(string debugTextParam)
    {
        debugText2.text = debugTextParam;
    }

}
