using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;
using System.Runtime.CompilerServices;

public class FireBullet : MonoBehaviour
{
    [SerializeField]
    private bool UseObjectPool = false;
    [SerializeField]
    private bool AddBulletSpread = true;
    [SerializeField]
    private Vector3 BulletSpreadVariance = new Vector3(0.1f, 0.1f, 0.1f);
    [SerializeField]
    private ParticleSystem ShootingSystem;
    [SerializeField]
    private Transform BulletSpawnPoint;
    [SerializeField]
    private ParticleSystem ImpactParticleSystem;
    [SerializeField]
    private TrailRenderer BulletTrail;
    [SerializeField]
    private float ShootDelay = 0.5f;
    [SerializeField]
    private LayerMask Mask;
    
    [SerializeField]
    private float BulletRange = 100;
    [SerializeField]
    private BulletSpawner bulletSpawner = null;

    private Animator Animator = null;
    private AudioSource audioSource = null;
    private float LastShootTime;

    //public float speed = 50f;
    //public GameObject bulletObj;
    //public Transform frontOfGun;
 
    public static event Action GunFired;

    private void Awake()
    {
        Animator = GetComponent<Animator>();
        audioSource = GetComponent<AudioSource>();
    }

    public void Fire()
    {
        

        //Raycast
        Shoot();


        //Bullets
        //GameObject spawnedBullet = Instantiate(bulletObj, frontOfGun.position, frontOfGun.rotation);
        //spawnedBullet.GetComponent<Rigidbody>().velocity = speed * frontOfGun.forward;
        //Destroy(spawnedBullet, 5f);
        GunFired?.Invoke();
    }

    private void Shoot()
    {
        if (LastShootTime + ShootDelay < Time.time)
        {
            audioSource.Play();
            // Use an object pool instead for these! To keep this tutorial focused, we'll skip implementing one.
            // For more details you can see: https://youtu.be/fsDE_mO4RZM or if using Unity 2021+: https://youtu.be/zyzqA_CPz2E

            //Animator.SetBool("IsShooting", true);
            ShootingSystem.Play();
            Vector3 direction = GetDirection();

            if (Physics.Raycast(BulletSpawnPoint.position, direction, out RaycastHit hit, float.MaxValue, Mask))
            {
                if (UseObjectPool)
                {
                    Bullet bullet = bulletSpawner.GetBullet();
                    TrailRenderer trail = bullet.GetComponent<TrailRenderer>();
                    
                    bullet.gameObject.transform.position = BulletSpawnPoint.position;
                    
                    bullet.SetTrail(trail, hit.point, hit.normal, true);
                    
                    //StartCoroutine(SpawnTrail(trail, hit.point, hit.normal, true));
                }
                else
                {
                    TrailRenderer trail = Instantiate(BulletTrail, BulletSpawnPoint.position, Quaternion.identity);
                    //StartCoroutine(SpawnTrail(trail, hit.point, hit.normal, true));
                }

                LastShootTime = Time.time;
            }
            // this has been updated to fix a commonly reported problem that you cannot fire if you would not hit anything
            else
            {
                if (UseObjectPool)
                {
                    Bullet bullet = bulletSpawner.GetBullet();
                    TrailRenderer trail = bullet.GetComponent<TrailRenderer>();
                    bullet.gameObject.transform.position = BulletSpawnPoint.position;

                    bullet.SetTrail(trail, BulletSpawnPoint.position + GetDirection() * BulletRange, Vector3.zero, false);

                    //StartCoroutine(SpawnTrail(trail, BulletSpawnPoint.position + GetDirection() * BulletRange, Vector3.zero, false));
                }
                else
                {
                    TrailRenderer trail = Instantiate(BulletTrail, BulletSpawnPoint.position, Quaternion.identity);
                    //StartCoroutine(SpawnTrail(trail, BulletSpawnPoint.position + GetDirection() * BulletRange, Vector3.zero, false));
                }

                LastShootTime = Time.time;
            }
        }
    }

    private Vector3 GetDirection()
    {
        Vector3 direction = BulletSpawnPoint.transform.forward;

        if (AddBulletSpread)
        {
            direction += new Vector3(
                UnityEngine.Random.Range(-BulletSpreadVariance.x, BulletSpreadVariance.x),
                UnityEngine.Random.Range(-BulletSpreadVariance.y, BulletSpreadVariance.y),
                UnityEngine.Random.Range(-BulletSpreadVariance.z, BulletSpreadVariance.z)
            );

            direction.Normalize();
        }

        return direction;
    }

    //private IEnumerator SpawnTrail(TrailRenderer Trail, Vector3 HitPoint, Vector3 HitNormal, bool MadeImpact)
    //{
    //    // This has been updated from the video implementation to fix a commonly raised issue about the bullet trails
    //    // moving slowly when hitting something close, and not
    //    Vector3 startPosition = Trail.transform.position;
    //    float distance = Vector3.Distance(Trail.transform.position, HitPoint);
    //    float remainingDistance = distance;

    //    while (remainingDistance > 0)
    //    {
    //        Trail.transform.position = Vector3.Lerp(startPosition, HitPoint, 1 - (remainingDistance / distance));

    //        remainingDistance -= BulletSpeed * Time.deltaTime;

    //        yield return null;
    //    }
    //    //Animator.SetBool("IsShooting", false);
    //    Trail.transform.position = HitPoint;
    //    if (MadeImpact)
    //    {
    //        Instantiate(ImpactParticleSystem, HitPoint, Quaternion.LookRotation(HitNormal));
    //    }

    //    //Destroy(Trail.gameObject, Trail.time);
    //}

}
