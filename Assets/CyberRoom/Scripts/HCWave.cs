using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class HCWave : MonoBehaviour
{
    public float FrontWave = 0.5f;
    public float SideWave = 1f;
    float scatter;

    void Awake()
    {
        UnityEngine.Random.InitState((int)System.DateTime.Now.Ticks * 1000);
    }
    // Start is called before the first frame update
    void Start()
    {
        scatter = Random.Range(-0.5f, 0.5f);
    }

    // Update is called once per frame
    void Update()
    {
        transform.localRotation = Quaternion.Euler(FrontWave * Mathf.Sin(Time.time * (2.8f + scatter)), transform.localRotation.x, SideWave * Mathf.Sin(Time.time * (3.4f + scatter)));
    }
}
