using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class HeadRayController : MonoBehaviour
{
    private LineRenderer lineRenderer = null;

    // Start is called before the first frame update
    void Awake()
    {
        lineRenderer = gameObject.GetComponent<LineRenderer>();
    }

    private void Start()
    {
        if (lineRenderer)
        {
            lineRenderer.enabled = true;
        }
    }

    // Update is called once per frame
    void Update()
    {
        RunRaycast();
    }

    private void RunRaycast()
    {
        Vector3 start = transform.position;
        Vector3 direction = transform.forward;
        float length = 5.0f;
        Ray ray = new Ray(start, direction);
        Vector3 endPos = start + (direction * length);

        if (Physics.Raycast(ray, out RaycastHit rayHit, length))
        {
            endPos = rayHit.point;
            Debug.Log(rayHit.collider.gameObject.name);
        }

        if (lineRenderer)
        {
            lineRenderer.SetPosition(0, start);
            lineRenderer.SetPosition(1, endPos);
        }
    }
}
