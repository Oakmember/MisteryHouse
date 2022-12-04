using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PixartXRGrabInteractable : XRGrabInteractable
{

    [SerializeField]
    private bool isGrabActivated = false;

    public bool IsGrabActivated => isGrabActivated;

    protected override void Grab()
    {
        if (!isGrabActivated)
        {
            isGrabActivated = true;

            base.Grab();
        }

    }

    protected override void Drop()
    {
        isGrabActivated = false;

        base.Drop();
    }

}
