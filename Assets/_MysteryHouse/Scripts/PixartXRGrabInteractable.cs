using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PixartXRGrabInteractable : XRGrabInteractable
{

    [SerializeField]
    private bool isGrabActivated = false;

    private PropController propController = null;

    public bool IsGrabActivated => isGrabActivated;


    protected override void Grab()
    {
        if (!isGrabActivated)
        {
            isGrabActivated = true;

            base.Grab();

            PropController propController = gameObject.GetComponent<PropController>();
            if (propController)
            {
                propController.Event_SetLayerNonInteractable();
            }
        }
    }

    protected override void Drop()
    {
        isGrabActivated = false;

        base.Drop();

        PropController propController = gameObject.GetComponent<PropController>();
        if (propController)
        {
            propController.Event_SetLayerInteractable();
        }
    }

}
