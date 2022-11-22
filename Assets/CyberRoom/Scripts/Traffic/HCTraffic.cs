using System.Collections;
using System.Collections.Generic;
using UnityEngine;

//IL.ranch, 2020. ILonion32@gmail.com
public class HCTraffic : MonoBehaviour
{
    [Header("maximum cars in whole scene:")]
    [Header("--------")]
    [Header("done.")]
    [Header("6. add HCVehicle.cs script on your vehicles;")]
    [Header("5. adjust intuitive params on HCTraffic.cs;")]
    [Header("4. add vehicles prefabs to 'Vehicle' slots (see below);")]
    [Header("3. add children GO's - car traffic spawn points;")]
    [Header("2. add HCTraffic.cs on it;")]
    [Header("1. make new GameObject;")]
    [Header("Usage:")]
    public int MaxVehicleCount = 50;
    [Header("spawn frequency (min time):")]
    public float MinSpawnInterval = 0.3f;
    [Header("spawn frequency (max time):")]
    public float MaxSpawnInterval = 1.3f;
    [Header("car start position scatter:")]
    public float MaxStartScatter = 3.0f;
    [Header("cars variety:")]
    public GameObject[] Vehicle;
    [Header("speed variety for cars line:")]
    public float CarLineSpeedVarMin = -5f;
    public float CarLineSpeedVarMax = 15f;

    [HideInInspector]
    public int VehicleCount;
    [HideInInspector]
    public Transform[] SpawnPoints;

    bool locked;
    float[] CarSpeedVariations;
    GameObject VehicleContainer;

    //collect all spawn points (must be children)
    void Awake()
    {
        if (transform.childCount != 0)
        {
            SpawnPoints = new Transform[transform.childCount];
        }
        else
        {
            Debug.Log(" <color=yellow> No spawn points! </color>");
            locked = true;
            return;
        }

        for (int k = 0; k < transform.childCount; k++)
        {
            SpawnPoints[k] = transform.GetChild(k);
        }
    }

    // Start is called before the first frame update
    void Start()
    {
        UnityEngine.Random.InitState((int)System.DateTime.Now.Ticks * 1000);
        StartCoroutine(SpawnInterval(Random.Range(MinSpawnInterval, MaxSpawnInterval)));
        VehicleContainer = new GameObject { };
        VehicleContainer.name = "VehicleContainer";

        if (!locked)
        {
            CarSpeedVariations = new float[SpawnPoints.Length];
            for(int k0 = 0; k0 < CarSpeedVariations.Length; k0++)
            {
                CarSpeedVariations[k0] = Random.Range(CarLineSpeedVarMin, CarLineSpeedVarMax);
            }
        }
    }

    // Update is called once per frame
    void Update()
    {
        //Debug.Log(VehicleCount);
    }

    IEnumerator SpawnInterval(float rTime)
    {
        yield return new WaitForSeconds(rTime);

        if (!locked)
        {
            if (VehicleCount < MaxVehicleCount)
            {
                for (int k0 = 0; k0 < SpawnPoints.Length; k0++)
                {
                    GameObject obj = Instantiate(Vehicle[Random.Range(0, Vehicle.Length)]);

                    obj.transform.position = SpawnPoints[k0].position + Random.insideUnitSphere * MaxStartScatter;
                    obj.transform.localRotation = SpawnPoints[k0].localRotation;

                    if (obj.transform.gameObject.GetComponent<HCVehicle>())
                    {
                        obj.transform.gameObject.GetComponent<HCVehicle>()._HCTrafficLinked = transform.gameObject.GetComponent<HCTraffic>();
                        obj.transform.gameObject.GetComponent<HCVehicle>().CarSpeed += CarSpeedVariations[k0];
                    }
                    else
                    {
                        Debug.Log(" <color=yellow> Wrong vehicle! 'HCVehicle' script is required </color>");
                        locked = true;
                        break;
                    }
                    obj.transform.parent = VehicleContainer.transform;
                    VehicleCount++;

                }
            }
            StartCoroutine(SpawnInterval(Random.Range(MinSpawnInterval, MaxSpawnInterval)));
        }
    }

    void OnDrawGizmos()
    {
        for (int k0 = 0; k0 < transform.childCount; k0++)
        {
            Vector3 Pos = transform.GetChild(k0).transform.localPosition;
            Gizmos.color = Color.green;
            Gizmos.DrawWireSphere(Pos, MaxStartScatter);
            Gizmos.DrawLine(transform.GetChild(k0).position, transform.GetChild(k0).position + transform.GetChild(k0).transform.TransformVector(Vector3.forward) * 300);
            Gizmos.color = Color.yellow;
            for (int k1 = 0; k1 < 10; k1++)
            {
                Gizmos.DrawWireSphere(Pos += transform.GetChild(k0).transform.TransformVector(Vector3.forward) * 30, MaxStartScatter);
            }
        }
    }
}
