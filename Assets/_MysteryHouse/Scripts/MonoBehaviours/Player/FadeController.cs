using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class FadeController : MonoBehaviour
{

    [SerializeField] LayerMask collisionLayer;
    [SerializeField] float fadeSpeed = 0.0f;
    [SerializeField] float sphereCheckSize = 0.15f;

    private Material cameraFadeMaterial = null;
    private bool isCameraFadeOut = false;

    private void Awake()
    {
        cameraFadeMaterial = GetComponent<Renderer>().material;
    }

    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        if (Physics.CheckSphere(transform.position, sphereCheckSize, collisionLayer, QueryTriggerInteraction.Ignore))
        {
            CameraFade(1.0f);
            isCameraFadeOut = true;
        }
        else
        {
            if (!isCameraFadeOut) return;
            CameraFade(0.0f);
        }
    }

    public void CameraFade(float targetAlpha)
    {
        var fadeValue = Mathf.MoveTowards(cameraFadeMaterial.GetFloat("_AlphaValue"), targetAlpha, Time.deltaTime * fadeSpeed);
        cameraFadeMaterial.SetFloat("_AlphaValue", fadeValue);

        if(fadeValue <= 0.01f) isCameraFadeOut = false;

    }

    private void OnDrawGizmos()
    {
        Gizmos.color = new Color(0.0f, 1.0f, 0.75f);
        Gizmos.DrawSphere(transform.position, sphereCheckSize);
    }
}
