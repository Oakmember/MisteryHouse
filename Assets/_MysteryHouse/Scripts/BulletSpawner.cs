using UnityEngine;
using UnityEngine.Pool;

public class BulletSpawner : MonoBehaviour
{
    [SerializeField]
    private Bullet Prefab;
    [SerializeField]
    private Transform spawnLocation = null;
    [SerializeField]
    private int BulletsPerSecond = 10;
    [SerializeField]
    private float Speed = 5f;
    [SerializeField]
    private bool UseObjectPool = false;

    private int bulletsToSpawn = 0;

    public ObjectPool<Bullet> BulletPool;

    private float LastSpawnTime;

    private Bullet Instance = null;

    private void Start()
    {

       
        //_pool = new ObjectPool<GameObject>(() => new GameObject("PooledObject"), (obj) => obj.SetActive(true), (obj) => obj.SetActive(false),(obj) => Destroy(obj), false, 10, 10);
        BulletPool = new ObjectPool<Bullet>(CreatePooledObject, OnTakeFromPool, OnReturnToPool, OnDestroyObject, false, 10, 10);
    }

    private Bullet CreatePooledObject()
    {
        
        Bullet instance = Instantiate(Prefab, Vector3.zero, Quaternion.identity);
        instance.Disable += ReturnObjectToPool;
        instance.gameObject.SetActive(false);

        return instance;
    }

    public Bullet GetBullet()
    {

        return BulletPool.Get();
    }

    private void ReturnObjectToPool(Bullet Instance)
    {
        BulletPool.Release(Instance);
    }

    private void OnTakeFromPool(Bullet Instance)
    {
        Instance.gameObject.SetActive(true);
        Instance.transform.SetParent(transform, true);
        Instance.gameObject.GetComponent<TrailRenderer>().enabled = true;
    }

    private void OnReturnToPool(Bullet Instance)
    {
        Instance.gameObject.SetActive(false);
    }

    private void OnDestroyObject(Bullet Instance)
    {
        Destroy(Instance.gameObject);
    }

    private void OnGUI()
    {
        if (UseObjectPool)
        {
            GUI.Label(new Rect(10, 10, 200, 30), $"Total Pool Size: {BulletPool.CountAll}");
            GUI.Label(new Rect(10, 30, 200, 30), $"Active Objects: {BulletPool.CountActive}");
        }
    }

    //private void Update()
    //{
    //    while (bulletsToSpawn < 10)
    //    {

    //        BulletPool.Get();

    //        bulletsToSpawn++;
    //    }

        
    //}

    //private void Update()
    //{
    //    float delay = 1f / BulletsPerSecond;
    //    if (LastSpawnTime + delay < Time.time)
    //    {
    //        int bulletsToSpawnInFrame = Mathf.CeilToInt(Time.deltaTime / delay);
    //        while (bulletsToSpawnInFrame > 0)
    //        {
    //            if (!UseObjectPool)
    //            {
    //                Bullet instance = Instantiate(Prefab, Vector3.zero, Quaternion.identity);
    //                instance.transform.SetParent(transform, true);

    //                SpawnBullet(instance);
    //            }
    //            else
    //            {
    //                BulletPool.Get();
    //            }

    //            bulletsToSpawnInFrame--;
    //        }

    //        LastSpawnTime = Time.time;
    //    }
    //}

    private void SpawnBullet(Bullet Instance)
    {
        //Vector3 spawnLocation = new Vector3(
        //    SpawnArea.transform.position.x + SpawnArea.center.x + Random.Range(-1 * SpawnArea.bounds.extents.x, SpawnArea.bounds.extents.x),
        //    SpawnArea.transform.position.y + SpawnArea.center.y + Random.Range(-1 * SpawnArea.bounds.extents.y, SpawnArea.bounds.extents.y),
        //    SpawnArea.transform.position.z + SpawnArea.center.z + Random.Range(-1 * SpawnArea.bounds.extents.z, SpawnArea.bounds.extents.z)
        //);

        Instance.transform.position = spawnLocation.position;

        Instance.Shoot(spawnLocation.position, spawnLocation.transform.forward, Speed);
    }






    //public Object prefab;
    //private IObjectPool<Object> objectPool;

    //void Start()
    //{
    //    objectPool = new ObjectPool<Object>(InstantiateObject, OnObject, OnReleased);
    //}

    //private Object InstantiateObject()
    //{
    //    Object obj = Instantiate(prefab);
    //    obj.SetPool(objectPool);
    //    return obj;
    //}

    //public void OnObject(Object obj)
    //{
    //    obj.gameObject.SetActive(true);
    //}

    //public void OnReleased(Object obj)
    //{
    //    obj.gameObject.SetActive(false);
    //}

    //void Update()
    //{
    //    objectPool.Get();
    //    //Instantiate(prefab);
    //}

}
