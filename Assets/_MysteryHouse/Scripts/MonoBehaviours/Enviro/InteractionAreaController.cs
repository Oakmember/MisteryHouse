using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class InteractionAreaController : MonoBehaviour
{
    [SerializeField]
    private GameObject interactionAreaTip = null;

    void Start()
    {
        SetInteractionAreaTip(false);
    }

    private void OnTriggerEnter(Collider other)
    {
        if (other.CompareTag(Consts.playerTag))
        {
            SetInteractionAreaTip(true);
        }
    }

    private void OnTriggerExit(Collider other)
    {
        if (other.CompareTag(Consts.playerTag))
        {
            SetInteractionAreaTip(false);
        }
    }

    private void SetInteractionAreaTip(bool isActivatedParam)
    {
        interactionAreaTip.SetActive(isActivatedParam);
    }
}
