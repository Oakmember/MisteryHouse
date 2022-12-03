using System.Collections;
using UnityEngine;

[RequireComponent(typeof(Rigidbody))]
public class Bullet : MonoBehaviour
{
    [SerializeField]
    private ParticleSystem ImpactSystem;
    [SerializeField]
    private ParticleSystem ImpactParticleSystem;
    [SerializeField]
    private float BulletSpeed = 100;

    private Rigidbody Rigidbody;
    public delegate void OnDisableCallback(Bullet Instance);
    public OnDisableCallback Disable;

    private void Awake()
    {
        Rigidbody = GetComponent<Rigidbody>();
    }

    public void Shoot(Vector3 Position, Vector3 Direction, float Speed)
    {
        Rigidbody.velocity = Vector3.zero;
        transform.position = Position;
        transform.forward = Direction;

        Rigidbody.AddForce(Direction * Speed, ForceMode.VelocityChange);
    }

    private void OnTriggerEnter(Collider other)
    {
        ImpactSystem.transform.forward = -1 * transform.forward;
        ImpactSystem.Play();
        Rigidbody.velocity = Vector3.zero;
    }

    private void OnParticleSystemStopped()
    {
        
        Disable?.Invoke(this);
    }

    public void SetTrail(TrailRenderer Trail, Vector3 HitPoint, Vector3 HitNormal, bool MadeImpact)
    {
        StartCoroutine(SpawnTrail(Trail, HitPoint, HitNormal, MadeImpact));
    }

    private IEnumerator SpawnTrail(TrailRenderer Trail, Vector3 HitPoint, Vector3 HitNormal, bool MadeImpact)
    {
        // This has been updated from the video implementation to fix a commonly raised issue about the bullet trails
        // moving slowly when hitting something close, and not
        Vector3 startPosition = Trail.transform.position;
        float distance = Vector3.Distance(Trail.transform.position, HitPoint);
        float remainingDistance = distance;

        while (remainingDistance > 0)
        {
            Trail.transform.position = Vector3.Lerp(startPosition, HitPoint, 1 - (remainingDistance / distance));

            remainingDistance -= BulletSpeed * Time.deltaTime;

            yield return null;
        }
        //Animator.SetBool("IsShooting", false);
        Trail.transform.position = HitPoint;
        if (MadeImpact)
        {
            Instantiate(ImpactParticleSystem, HitPoint, Quaternion.LookRotation(HitNormal));
        }
        Trail.enabled = false;
        Debug.Log("sssssssssssssss");
        Disable?.Invoke(this);
        //Destroy(Trail.gameObject, Trail.time);
    }
}
