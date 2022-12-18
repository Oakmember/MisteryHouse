using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using EPOOutline;

public class HighlightController : MonoBehaviour
{
    [SerializeField]
    private Outlinable outlinable = null;

    

    // Start is called before the first frame update
    void Start()
    {
        StartCoroutine(InitCor());
    }

    IEnumerator InitCor()
    {
        yield return new WaitForSeconds(5.0f);
        outlinable.enabled = false;
    }

    private void OnTriggerEnter(Collider other)
    {

        if (other.gameObject.layer == LayerMask.NameToLayer(Consts.handsLayer))
        {
            if (outlinable) {
                outlinable.enabled = true;
            }
        }
    }

    private void OnTriggerExit(Collider other)
    {

        if (other.gameObject.layer == LayerMask.NameToLayer(Consts.handsLayer))
        {
            if (outlinable)
            {
                outlinable.enabled = false;
            }
        }
    }
}
