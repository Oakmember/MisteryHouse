using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PixartXRGrabInteractable : XRGrabInteractable
{
    [SerializeField]
    private Transform leftAttachTransform = null;

    [SerializeField]
    private Transform rightAttachTransform = null;

    [SerializeField]
    private bool isGrabActivated = false;

    private PropController propController = null;

    public bool IsGrabActivated => isGrabActivated;


    protected override void OnSelectEntered(SelectEnterEventArgs args)
    {
        if (args.interactorObject.transform.CompareTag("LeftHand"))
        {
            attachTransform = leftAttachTransform;
        }else if (args.interactorObject.transform.CompareTag("RightHand")) {
            attachTransform = rightAttachTransform;
        }

        base.OnSelectEntered(args);

        GrabItem();

    }
    protected override void OnSelectExited(SelectExitEventArgs args)
    {
        base.OnSelectExited(args);

        DropItem();
    }

    protected void GrabItem()
    {
        if (!isGrabActivated)
        {
            isGrabActivated = true;

            //base.Grab();

            PropController propController = gameObject.GetComponent<PropController>();
            if (propController)
            {
                propController.Event_SetLayerNonInteractable();
            }
        }
    }

    protected void DropItem()
    {
        isGrabActivated = false;

        //base.Drop();

        PropController propController = gameObject.GetComponent<PropController>();
        if (propController)
        {
            propController.Event_SetLayerInteractable();
        }
    }

}
