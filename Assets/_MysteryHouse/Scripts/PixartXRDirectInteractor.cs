using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

public class PixartXRDirectInteractor : XRDirectInteractor
{
    // Start is called before the first frame update

    protected override void OnEnable()
    {
        base.OnEnable();
       
    }

    protected override void OnSelectEntered(SelectEnterEventArgs args)
    {
        base.OnSelectEntered(args);


        IXRSelectInteractable BaseInteractable = args.interactableObject;
        if (BaseInteractable != null)
        {
            //Debug.Log(BaseInteractable.transform.gameObject.name);

            if (BaseInteractable.transform.gameObject)
            {
                PropController propController = BaseInteractable.transform.gameObject.GetComponent<PropController>();
                if (propController != null)
                {
                    propController.PropStateType = PropStateType.Grabbed;
                }
            }
        }
    }

    protected override void OnSelectExited(SelectExitEventArgs args)
    {
        base.OnSelectExited(args);

        IXRSelectInteractable BaseInteractable = args.interactableObject;
        if (BaseInteractable != null)
        {
            //Debug.Log(BaseInteractable.transform.gameObject.name);

            if (BaseInteractable.transform.gameObject)
            {
                PropController propController = BaseInteractable.transform.gameObject.GetComponent<PropController>();
                if (propController != null)
                {
                    propController.PropStateType = PropStateType.Dropped;
                }
            }
        }
    }

   
}
